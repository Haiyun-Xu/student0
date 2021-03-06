/**
 * This module contains helper functions for general use.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "helpers.h"

/**
 * Duplicate the given string.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param string The string to duplicate
 * 
 * @return char* A duplicate of the given string
 */
char *duplicate_string(const char *string) {
  size_t length = strlen(string);
  char *newString = (char*) malloc((length + 1) * sizeof(char));
  if (newString == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    return NULL;
  }

  strcpy(newString, string);
  return newString;
}

/**
 * Concatenate the two strings together, without modifying the originals.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param prefix The string to be in the front
 * @param suffix The string to be in the back
 * 
 * @return char* The concatenated string
 */
char *concatenate_strings(const char *prefix, const char *suffix) {
  size_t combinedLength = strlen(prefix) + strlen(suffix);
  char *combinedString = (char*) malloc((combinedLength + 1) * sizeof(char));
  if (combinedString == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    return NULL;
  }

  strcpy(combinedString, prefix);
  strcat(combinedString, suffix);
  return combinedString;
}

/**
 * Join the strings together in the given order, separated by the separator.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param strings An array of strings, terminated by a NULL pointer
 * @param separator The character used to separate the strings
 * 
 * @return char* The joined string
 */
char *join_strings(char *const *strings, const char separator) {
  if (strings == NULL)
    return NULL;
  
  /*
   * count the combined length of all the strings, as well as the number of strings.
   * the memory required for the joined string is: sum(length of each string)
   * + a separator for each string (null char for the last string)
   */
  int joinedStringLength = 0, numOfStrings = 0;
  char *string;
  while ((string = strings[numOfStrings++]) != NULL) {
    joinedStringLength += strlen(string);
  }
  char *joinedStrings = (char *) malloc((joinedStringLength + numOfStrings) * sizeof(char));
  if (joinedStrings == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    return NULL;
  }
  
  // join the strings
  joinedStringLength = 0;
  numOfStrings = 0;
  while((string = strings[numOfStrings++]) != NULL) {
    strcpy((joinedStrings + joinedStringLength), string);
    joinedStringLength += strlen(string);

    // all strings should terminate with the separator char
    joinedStrings[joinedStringLength] = separator;
    joinedStringLength += 1;
  }
  // the last string should terminate with the null char
  joinedStrings[joinedStringLength - 1] = '\0';

  return joinedStrings;
}

/**
 * Clean the entire string by overwriting it with null char.
 * 
 * @param string The string to be cleaned
 * @param length The length of the string
 */
void clean_string(char *string, int length) {
  // edge cases
  if (string == NULL || length <= 0) {
    return;
  }

  for (int index = 0; index < length; index++) {
    string[index] = '\0';
  }
  return;
}

/**
 * Checks whether the tokens list is empty.
 * 
 * @param tokens The list of command tokens
 * 
 * @return Returns 1 if the tokens list is empty, or 0 otherwise
 */
int is_tokens_empty(struct tokens *tokens) {
  // edge cases
  if (tokens == NULL) {
    return 0;
  }

  char *argument  = tokens_get_token(tokens, 0);
  if (argument == NULL) {
    return 1;
  } else {
    return 0;
  }
}

/**
 * Checks whether the string is a non-negative integer.
 * 
 * @param string The string
 * 
 * @return int Returns the integer if the conversion is possible, or return -1
 */
int is_integer(const char *string) {
  // edge cases
  if (string == NULL) {
    return -1;
  }

  // check that each of the character is a number
  int argLength = strlen(string);
  for (int index = 0; index < argLength; index++) {
    if (!isdigit(string[index])) {
      return -1;
    }
  }

  int integer = atoi(string);
  if (integer >= 0) {
    return integer;
  } else {
    return -1;
  }
}

/**
 * Checks whether the program should be executed in the background. A program
 * with the background execution flag always has the form "[command] &".
 * 
 * @param tokens The list of command tokens
 * 
 * @return int Returns 1 if there's the program should be executed in the
 *             background, or 0 otherwise
 */
int should_execute_in_background(struct tokens *tokens) {
  // edge cases
  if (tokens == NULL) {
    return 0;
  }

  size_t tokenLength = tokens_get_length(tokens);
  char *argument = tokens_get_token(tokens, tokenLength - 1);
  if (strcmp(argument, "&") == 0) {
    return 1;
  } else {
    return 0;
  }
}

/**
 * Checks whether the file is executable.
 * 
 * @param filePath The full path of the file
 * 
 * @return int Returns 0 if the file is executable, or 1 if not;
 */
int is_file_executable(const char* filePath) {
  if (access(filePath, F_OK))
    return 1; // no such file or directory

  // get the file status
  struct stat fileStat;
  stat(filePath, &fileStat);

  if (!S_ISREG(fileStat.st_mode))
    return 1; // not a regular file

  if (access(filePath, X_OK))
    return 1; // no permission to execute file

  return 0;
}

/**
 * Returns the full path of the given executable file.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param fileName Name of the executable file
 * 
 * @return char* The full path of the executable file, or NULL if the full
 *               path is not found, i.e. the executable file does not exist
 */
char *resolve_executable_full_path(const char *fileName) {
  // edge cases
  if (fileName == NULL)
    return NULL;

  char *delimiter = ":";
  if (fileName[0] == '/' && is_file_executable(fileName) == 0) {
    /*
     * if the file name is already a path and links to an executable file,
     * we copy and return the file name
     */
    return duplicate_string(fileName);
  } else {
    /*
     * otherwise, we find the full path of the executable file if it exists,
     * by using the directory prefixes in the PATH environment variable
     */
    char *envVarPATH = getenv("PATH");
    envVarPATH = duplicate_string(envVarPATH); /** TODO: remember to free this */

    // prefix the file name with a '/' path divider
    size_t fileNameLength = strlen(fileName) + 1;
    char *newFileName = (char*) malloc((fileNameLength + 1) * sizeof(char)); /** TODO: remember to free this */
    if (newFileName == NULL) {
      perror("Failed to allocate memory: ");
      free(envVarPATH);
      return NULL;
    }
    newFileName[0] = '/';
    strcpy((newFileName + 1), fileName);

    // tokenize the PATH environment variable in place
    char *directoryPrefix = strtok(envVarPATH, delimiter), *fullPath = NULL;
    do {
      // concatenat the firectory prefix and the file name
      fullPath = concatenate_strings(directoryPrefix, newFileName); /** TODO: remember to free this */
      
      if (is_file_executable(fullPath) == 0) {
        break; // return the first full path of the executable file
      } else {
        free(fullPath);
        fullPath = NULL;
      }
    } while ((directoryPrefix = strtok(NULL, delimiter)) != NULL);
    free(newFileName);
    free(envVarPATH);

    // fullPath can be NULL if the executable file is not found
    return fullPath;
  }
}