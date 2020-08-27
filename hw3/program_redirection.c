/**
 * This module contains helper functions for I/O redirected programs.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "program_redirection.h"

/**
 * Checks whether the given argument is the redirect symbol.
 * 
 * @param argument An argument string
 * 
 * @return int Returns -1 if the argument is an input redirection symbol,
 *             returns 1 if it's an output redirection symbol, and 0 if
 *             not a redirection symbol
 */
int is_redirect_symbol(const char* argument) {
  // edge cases
  if (argument == NULL)
    return 0;

  if (strcmp(argument, "<") == 0) {
    return -1;
  } else if (strcmp(argument, ">") == 0) {
    return 1;
  } else {
    return 0;
  }
}

/**
 * Checks whether the command line contains input/output redirection.
 * Redirection is always of the form "[prgram] > [file]" or "[prgram] < [file]".
 * 
 * @param tokens The list of command tokens
 * 
 * @return int Returns -1/1 if there's input/output redirection, or 0 if no redirection
 */
int contains_redirection(struct tokens *tokens) {
  // edge cases
  if (tokens == NULL)
    return 0;
  
  char *argument = NULL;
  int redirection = 0;
  size_t tokensLength = tokens_get_length(tokens);
  for (size_t index = 0; index < tokensLength; index++) {
    // iterate through the command arguments and check if any of them is a redirection symbol
    argument = tokens_get_token(tokens, index);
    if ((redirection = is_redirect_symbol(argument))) {
      return redirection;
    }
  }
  return redirection;
}

/**
 * Return the redirection file name from the command tokens. The returned
 * string is not newly-allocated.
 * 
 * @param tokens The list of command tokens
 * @param fileNamePtr The address of the file name
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_redirection_file_name(struct tokens *tokens, char **fileNamePtr) {
  // edge cases
  if (tokens == NULL || fileNamePtr == NULL) {
    *fileNamePtr = NULL;
    return -1;
  }
  
  char *token = NULL;
  bool redirectionEncountered = false;
  size_t tokensLength = tokens_get_length(tokens);
  for (size_t index = 0; index < tokensLength; index++) {
    // iterate through the command arguments, until a redirection symbol is found
    token = tokens_get_token(tokens, index);
    if (is_redirect_symbol(token)) {
      redirectionEncountered = true;
    } else if (redirectionEncountered) {
      // in valid syntax, the file always follows the redirection symbol
      *fileNamePtr = token;
      return 0;
    }
  }
  // no redirection symbol was found, so no file name extracted
  return -1;
}

/**
 * Return the redirected program name from the command tokens. The returned
 * string is not newly-allocated.
 * 
 * @param tokens The list of command tokens
 * @param programNamePtr The address of the program name
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_redirected_program_name(struct tokens *tokens, char **programNamePtr) {
  // edge cases
  if (tokens == NULL || programNamePtr == NULL) {
    *programNamePtr = NULL;
    return -1;
  }

  // in valid syntax, the first token should be the program name
  *programNamePtr = tokens_get_token(tokens, 0);
  return 0;
}

/**
 * Return the program arguments from the command tokens in a Null terminated
 * argument list. The returned arguments are not newly-allocated, but the
 * argument list is.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param tokens The list of command tokens
 * @param argListPtr Address of a pointer to the list of program call arguments
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_redirected_program_argument(struct tokens *tokens, char ***argListPtr) {
  // edge cases
  if (tokens == NULL || argListPtr == NULL) {
    *argListPtr = NULL;
    return -1;
  }
  
  // count the number of program arguments in the command tokens
  size_t tokensLength = tokens_get_length(tokens);
  int argListLength = 0;
  char *token = NULL;
  for (size_t index = 0; index < tokensLength; index++) {
    token = tokens_get_token(tokens, index);
    if (is_redirect_symbol(token)) {
      break;
    } else {
      argListLength ++;
    }
  }

  // allocate memory for argument list
  char **argList = (char **) malloc((argListLength + 1) * sizeof(char *)); /** TODO: remember to free this */
  if (argList == NULL) {
    perror("Failed to allocate memory: ");
    return -1;
  }
  // the argument list is terminated by NULL;
  argList[argListLength] = NULL;

  // extract all argument tokens into the argument list
  for (int index = 0; index < argListLength; index++) {
    argList[index] = tokens_get_token(tokens, index);
  }

  *argListPtr = argList;
  return 0;
}

/**
 * Parse the command tokens into a program name, an argument list, and a
 * redirection file name. The returned program name, arguments, and file
 * name are not newly-allocated, but the argument list is.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param tokens The command tokens
 * @param programNamePtr The address of the program name
 * @param argListPtr The address of the argument list
 * @param fileNamePtr The address of the redirection file name
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int parse_redirection_tokens(struct tokens *tokens, char **programNamePtr, char ***argListPtr, char **fileNamePtr) {
  // edge cases
  if (tokens == NULL || programNamePtr == NULL || argListPtr == NULL || fileNamePtr == NULL) {
    *programNamePtr = NULL;
    *argListPtr = NULL;
    *fileNamePtr = NULL;
    return -1;
  }

  /* 
   * parse and extract the redirected program's name and arguments, as well as
   * the redirection file's name
   */
  int result = 0;
  result = get_redirection_file_name(tokens, fileNamePtr);
  result = result | get_redirected_program_name(tokens, programNamePtr);
  if (result != 0) {
    *programNamePtr = NULL;
    *argListPtr = NULL;
    *fileNamePtr = NULL;
    return -1;
  }

  result = get_redirected_program_argument(tokens, argListPtr);
  if (result != 0) {
    *programNamePtr = NULL;
    *argListPtr = NULL;
    *fileNamePtr = NULL;
    return -1;
  }

  return 0;
}

/**
 * Execute the given program with the given arguments, accounting for the
 * redirection options.
 * 
 * Takes the ownership of argument list and frees it.
 * 
 * @param programName The program name
 * @param programArgList The list of program arguments; will be freed by this function
 * @param redirectionType The redirection type: -1 for input redirect, 1 for
 *                        output redirect, and 0 for no redirect
 * @param redurectionFileName The redirection file name
 * 
 * @return int Returns 0 if the program executed successfully, or -1 otherwise
 */
int execute_redirected_program(const char *programName, char **programArgList, int redirectionType, const char *redirectionFileName) {
  // edge cases
  if (programName == NULL || programArgList == NULL) {
    return -1;
  }
  if (redirectionType && redirectionFileName == NULL) {
    fprintf(stderr, "Redirecting input/out without valid file name\n");
    free(programArgList);
    return -1;
  }
  const int INPUT_REDIRECTION = -1;
  const int OUTPUT_REDIRECTION = 1;

  // resolve the full path of the program executable
  char *programFullPath = resolve_executable_full_path(programName); /** TODO: remember to free this */
  if (programFullPath == NULL) {
    fprintf(stderr, "No such executable program\n");
    free(programArgList);
    return -1;
  }

  // execute the program as a child process
  pid_t processID = fork();
  if (processID == -1) {
    perror("Failed to create new process: ");
    free(programFullPath);
    free(programArgList);
    return -1;
  } else if (processID == 0) {
    /*
      * this is the child process, and it should be run with stdin/stdout
      * redirection. The child process should exit via the new process image,
      * so there's no need to return
      */
    // open the redirection file
    int redirectionFileDescriptor = -1;
    if (redirectionType == INPUT_REDIRECTION) {
      redirectionFileDescriptor = open(redirectionFileName, O_RDONLY);
    } else if (redirectionType == OUTPUT_REDIRECTION) {
      /*
        * if it's output redirection, create the file if it doesn't exist,
        * or truncate the file if it exist. When creating the file, set its
        * access permission to 664
        */
      redirectionFileDescriptor = open(redirectionFileName, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    }
    if (redirectionFileDescriptor == -1) {
      fprintf(stderr, "Failed to open file: %s\n", redirectionFileName);
      perror("Input/output redirection failed: ");
      /*
      * since this is the child process, we can't return to the caller;
      * so even if we free the program call arguments, there might be more
      * heap memory in the stack that will be leaked. So might as well leave
      * the heap memory unfreed, and let garbage collector handle it
      */
      exit(1);
    }
    
    /*
    * overwrite the standard input/output file descriptor
    */
    int redirectionResult = -1;
    if (redirectionType == INPUT_REDIRECTION) {
      redirectionResult = dup2(redirectionFileDescriptor, STDIN_FILENO);
    } else if (redirectionType == OUTPUT_REDIRECTION) {
      redirectionResult = dup2(redirectionFileDescriptor, STDOUT_FILENO);
    }

    if (redirectionResult == -1) {
      fprintf(stderr, "Failed to overwrite input/output\n");
      perror("Input/output redirection failed: ");
      // not freeing the heap memory for the same reason as explained above
      exit(1);
    } else {
      close(redirectionFileDescriptor);
    }

    // execute the requested program with I/O redirection
    execv(programFullPath, (char *const *) programArgList);
  }

  /*
   * this is the parent process, which returns only after waiting for the
   * child process to terminate
   */
  int terminatedProcessID = waitpid(processID, NULL, 0);
  if (terminatedProcessID == -1) {
    perror("Failed to wait for child process to terminate: ");
    free(programFullPath);
    free(programArgList);
    return -1;
  }
  
  free(programFullPath);
  free(programArgList);
  return 0;
}