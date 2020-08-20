/**
 * This module contains helper functions for non built-in programs.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "program_helpers.h"

/**
 * Duplicate the given string.
 * Caller should own the heap memory and free it.
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
 * Caller should own the heap memory and free it.
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
 * Caller should own the heap memory and free it.
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
 * Return the program's name.
 * 
 * @param tokens The list of command arguments
 * 
 * @return char* The program name in string
 */
char *get_program_name(struct tokens *tokens) {
  if (tokens == NULL)
    return NULL;

  return tokens_get_token(tokens, 0);
}

/**
 * Return the program's full path on the heap.
 * Caller should own the heap memory and free it.
 * 
 * @param tokens The list of command arguments
 * @param programName The name of the program
 * 
 * @return int Returns 0 if successful, or -1 if failed
 */
int get_program_full_path(struct tokens *tokens, char **programFullPath) {
  // edge cases
  if (tokens == NULL || programFullPath == NULL)
    return -1;

  char *delimiter = ":";
  char *programName = get_program_name(tokens);

  if (programName[0] == '/' && is_file_executable(programName) == 0) {
    /*
     * if the program name is already a path and links to an executable file,
     * we copy and return the program name
     */
    *programFullPath = duplicate_string(programName);
    return 0;
  } else {
    /*
     * otherwise, we find the full path of the program if it exists, by using
     * the directory prefixes in the PATH environment variable
     */
    char *envVarPATH = getenv("PATH"), *fullPath = NULL;
    envVarPATH = duplicate_string(envVarPATH); /** TODO: remember to free this */

    size_t programNameLength = strlen(programName) + 1;
    char *newProgramName = (char*) malloc((programNameLength + 1) * sizeof(char));
    if (newProgramName == NULL) {
      fprintf(stderr, "Failed to allocate memory\n");
      free(envVarPATH);
      *programFullPath = NULL;
      return -1;
    }

    // first need to prefix the programName with a directory separator
    newProgramName[0] = '/';
    strcpy((newProgramName + 1), programName);

    char *directoryPrefix = strtok(envVarPATH, delimiter);
    do {
      fullPath = concatenate_strings(directoryPrefix, newProgramName); /** TODO: remember to free this */
      
      if (is_file_executable(fullPath) == 0) {
        break; // return the program's first executable full path
      } else {
        free(fullPath);
        fullPath = NULL;
      }
    } while ((directoryPrefix = strtok(NULL, delimiter)));
    free(newProgramName);
    free(envVarPATH);

    // fullPath can be NULL if it's not found
    *programFullPath = fullPath;
    return 0;
  }
}

/**
 * Get the program arguments from the command.
 * The program arguments are not newly-allocated, but the pointer array to the program arguments is.
 * Caller should own the heap memory and free it.
 * 
 * @param tokens The list of command arguments
 * @param argList The address of a pointer to a list of strings
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_program_arguments(struct tokens *tokens, char ***argList) {
  // edge cases
  if (argList == NULL || tokens == NULL)
    return -1;
  
  /*
   * extract each of the program arguments; note that the first argument must
   * be the program name, and the last argument must be a NULL pointer
   */
  size_t argListLength = tokens_get_length(tokens);
  char **arguments = (char **) malloc((argListLength + 1) * sizeof(char*));
  if (arguments == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    *argList = NULL;
    return -1;
  }

  for (size_t index = 0; index < argListLength; index++) {
    arguments[index] = tokens_get_token(tokens, index);
  }
  // the argument list must be terminated by a NULL pointer
  arguments[argListLength] = NULL;
  *argList = arguments;

  return 0;
}

/**
 * Interpret command arguments of program call, and resolve the program full
 * path and the list of program parameters.
 * Caller should own the heap memory and free it.
 * 
 * @param tokens The list of command arguments
 * @param programFullPath Address of a string, where the program full path will be stored
 * @param programArgList Address of an array of strings, where the program call arguments will be stored
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int interpret_command_arguments(struct tokens *tokens, char **programFullPath, char ***programArgList) {
  // edge cases
  if (tokens == NULL) {
    *programFullPath = NULL;
    *programArgList = NULL;
    return -1;
  }

  // get the program name and check if its executable full path can be found
  char *programName = get_program_name(tokens);
  get_program_full_path(tokens, programFullPath);
  if (programFullPath == NULL) {
    /*
     * if the program's executable full path cannot be found,
     * print a warning and return -1
     */
    fprintf(stderr, "%s: Program not found\n", programName);
    *programFullPath = NULL;
    *programArgList = NULL;
    return -1;
  }

  // get the program arguments
  get_program_arguments(tokens, programArgList);
  return 0;
}

/**
 * Execute the given program with the given arguments.
 * 
 * @param programFullPath The full path to the program executable
 * @param programArgList The list of program arguments
 * 
 * @return int Returns 0 if the program executed successfully, or -1 otherwise
 */
int execute_program(const char *programFullPath, char *const *programArgList) {
  // edge cases
  if (programFullPath == NULL || programArgList == NULL) {
    return -1;
  }

  // execute the program as a child process
  pid_t processID = fork();
  if (processID == -1) {
    perror("Failed to create new process: ");
    return -1;
  } else if (processID == 0) {
    /*
      * this is the child process, so execute the requested program. The child
      * process should exit via the new process image, so there's no need to return
      */
    execv(programFullPath, programArgList);
  }

  /*
   * this is the parent process, which returns only after waiting for the
   * child process to terminate
   */
  int terminatedProcessID = waitpid(processID, NULL, 0);
  if (terminatedProcessID == -1) {
    perror("Failed to wait for child process to terminate: ");
    return -1;
  }
  
  return 0;
}