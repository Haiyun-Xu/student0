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