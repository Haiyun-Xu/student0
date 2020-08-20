/**
 * This module contains helper functions for I/O redirected programs.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "program_redirection.h"

/**
 * Checks whether the given argument is the redirection symbol.
 * 
 * @param argument An argument string
 * 
 * @return int Returns -1 if the argument is an input redirection symbol,
 *             returns 1 if it's an output redirection symbol, and 0 if
 *             not a redirection symbol
 */
int is_redirection_symbol(const char* argument) {
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
 * @param tokens The list of command arguments
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
    if ((redirection = is_redirection_symbol(argument))) {
      return redirection;
    }
  }
  return redirection;
}

/**
 * Get the redirected program call from the command.
 * The returned token struct is newly-allocated, and the
 * caller should own the heap memory and free it.
 * 
 * @param tokens The list of command arguments
 * @param programCallTokens Address of a pointer to the list of program call arguments
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_redirection_program_call(struct tokens *tokens, struct tokens **programCallTokens) {
  // edge cases
  if (tokens == NULL || programCallTokens == NULL) {
    *programCallTokens = NULL;
    return -1;
  }
  
  /*
   * iterate through the original tokens, and extract the arguments that belong
   * to the program call; the number of program call arguments must be less
   * than the total number of command line arguments
   */
  char *argument = NULL;
  size_t tokenLength = tokens_get_length(tokens);
  char **programCallArguments = (char**) malloc(tokenLength * sizeof(char*)); /** TODO: remember to free this */
  if (programCallArguments == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    *programCallTokens = NULL;
    return -1;
  }

  for (size_t index = 0; index < tokenLength; index++) {
    /*
     * if the current argument is a redirection symbol, terminate the array
     * with a NULL pointer; otherwise, append the argument to the list of
     * program call arguments
     */
    argument = tokens_get_token(tokens, index);
    if (is_redirection_symbol(argument)) {
      programCallArguments[index] = NULL;
      break;
    } else {
      programCallArguments[index] = argument;
    }
  }

  // join the program call arguments by space, and retokenize the program call
  char *programCall = join_strings(programCallArguments, ' '); /** TODO: remember to free this */
  free(programCallArguments);
  *programCallTokens = tokenize(programCall);
  free(programCall);

  return 0;
}

/**
 * Get the redirect file name from the command.
 * The returned string is not newly-allocated.
 * 
 * @param tokens The list of command arguments
 * @param fileName Address of a pointer to the filename
 * 
 * @return int Returns -1/1 if there's input/output redirection, or 0 if no redirection
 */
int get_redirection_file_name(struct tokens *tokens, char **fileName) {
  // edge cases
  if (tokens == NULL || fileName == NULL) {
    *fileName = NULL;
    return 0;
  }
  
  char *argument = NULL;
  int redirection = 0;
  size_t tokensLength = tokens_get_length(tokens);
  for (size_t index = 0; index < tokensLength; index++) {

    // iterate through the command arguments, until a redirection symbol is found
    argument = tokens_get_token(tokens, index);
    if ((redirection = is_redirection_symbol(argument))) {
      // in valid syntax, the file always follows the redirection symbol
      *fileName = tokens_get_token(tokens, index + 1);
      return redirection;
    }
  }
  return redirection;
}

/**
 * Execute the given program with the given arguments, accounting for the redirection options.
 * 
 * @param programFullPath The full path to the program executable
 * @param programArgList The list of program arguments
 * @param redirectionType The redirection type: -1 for input redirect, 1 for
 *                        output redirect, and 0 for no redirect
 * @param redurectionFileName The redirection file name
 * 
 * @return int Returns 0 if the program executed successfully, or -1 otherwise
 */
int execute_program_redirected(const char *programFullPath, char *const *programArgList, int redirectionType, const char *redirectionFileName) {
  // edge cases
  if (programFullPath == NULL || programArgList == NULL) {
    return -1;
  }
  if (redirectionType && redirectionFileName == NULL) {
    fprintf(stderr, "Redirecting input/out without valid file name\n");
    return -1;
  }
  const int INPUT_REDIRECTION = -1;
  const int OUTPUT_REDIRECTION = 1;

  // execute the program as a child process
  pid_t processID = fork();
  if (processID == -1) {
    perror("Failed to create new process: ");
    return -1;
  } else if (processID == 0) {
    /*
      * this is the child process, and it should be run with stdin/stdout
      * redirection if the redirection option was specified. The child process
      * should exit via the new process image, so there's no need to return
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
      exit(1);
    } else {
      close(redirectionFileDescriptor);
    }

    // with or without redirect, execute the requested program
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