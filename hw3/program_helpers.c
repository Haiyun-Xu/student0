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
  strcpy(combinedString, prefix);
  strcat(combinedString, suffix);
  return combinedString;
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
 * Return the program's full path on the heap.
 * Caller should own the heap memory and free it.
 * 
 * @param programName The name of the program
 * 
 * @return char* Pointer to the full path of the program, or NULL if it can't be found
 */
char *get_program_full_path(char *programName) {
  char *fullPath = NULL, *delimiter = ":";

  if (programName[0] == '/' && is_file_executable(programName) == 0) {
    /*
     * if the program name is already a path and links to an executable file,
     * we copy and return the program name
     */
    fullPath = duplicate_string(programName);
  } else {
    /*
     * otherwise, we find the full path of the program if it exists, by using
     * the directory prefixes in the PATH environment variable
     */
    char *envVarPATH = getenv("PATH");
    envVarPATH = duplicate_string(envVarPATH); /** TODO: remember to free this */

    // first need to prefix the programName with a directory separator
    size_t programNameLength = strlen(programName) + 1;
    char *newProgramName = (char*) malloc((programNameLength + 1) * sizeof(char));
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
  }

  // fullPath can be NULL if it's not found
  return fullPath;
}

/**
 * Get the program name from the command.
 * The returned string is not newly-allocated.
 * 
 * @param programName The address of a pointer to string
 * @param tokens The tokens struct containing all the command parts
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_program_name(char **programName, struct tokens *tokens) {
  // edge cases
  if (programName == NULL|| tokens == NULL)
    return -1;

  *programName = tokens_get_token(tokens, 0);
  return 0;
}

/**
 * Get the program arguments from the command.
 * The program arguments are not newly-allocated, but the pointer array to the program arguments is.
 * Caller should own the heap memory and free it.
 * 
 * @param argList The address of a pointer to a list of strings
 * @param tokens The tokens struct containing all the command parts
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_program_arguments(char ***argList, struct tokens *tokens) {
  // edge cases
  if (argList == NULL || tokens == NULL)
    return -1;
  
  /*
   * extract each of the program arguments; note that the first argument must
   * be the program name, and the last argument must be a NULL pointer
   */
  size_t argListLength = tokens_get_length(tokens);
  char **arguments = (char **) malloc((argListLength + 1) * sizeof(char*));
  for (size_t index = 0; index < argListLength; index++) {
    arguments[index] = tokens_get_token(tokens, index);
  }
  // the argument list must be terminated by a NULL pointer
  arguments[argListLength] = NULL;
  *argList = arguments;

  return 0;
}

/**
 * Execute non built-in programs as a new process.
 * 
 * @param tokens The tokens struct containing all the command parts
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int exec_program(struct tokens *tokens) {
  // edge cases
  if (tokens == NULL) {
    return -1;
  }

  char *programName, **argList, *programFullPath;

  // get the program name and check if its executable full path can be found
  get_program_name(&programName, tokens);
  programFullPath = get_program_full_path(programName);
  if (programFullPath == NULL) {
    /*
     * if the program's executable full path cannot be found,
     * print a warning and return -1
     */
    printf("%s: Command not found\n", programName);
    return -1;
  }

  // get the program arguments
  get_program_arguments(&argList, tokens);  /** TODO: remember to free this */
  
  // execute the program as a child process
  pid_t processID = fork();
  if (processID == -1) {
    perror("Failed to create new process: ");
    free(argList);
    return -1;
  } else if (processID == 0) {
    /*
     * this is the child process, should exit via code in the new process image
     * no need to free the program arguments
     */
    execv(programFullPath, argList);
  }

  // this is the parent process
  free(argList);
  int terminatedProcessID = waitpid(processID, NULL, 0); // wc program doesn't take the argument and so hangs to wait for stdin 
  if (terminatedProcessID == -1) {
    perror("Failed to wait for child process to terminate: ");
    return -1;
  }

  return 0;
}
