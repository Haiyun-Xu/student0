/**
 * This module contains helper functions for piped programs.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "program_piping.h"

/**
 * Checks whether the given argument is the pipe symbol.
 * 
 * @param argument An argument string
 * 
 * @return int Returns 1 if the argument is a pipe symbol, or 0 if not
 */
int is_pipe_symbol(const char *argument) {
  // edge cases
  if (argument == NULL)
    return 0;

  if (strcmp(argument, "|") == 0)
    return 1;
  else
    return 0;
}

/**
 * Checks whether the command line contains input/output piping.
 * Piping is always of the form "[prgram A] | [program B]".
 * 
 * @param tokens The list of command tokens
 * 
 * @return int Returns 1 if there's input/output piping, or 0 if not
 */
int contains_piping(struct tokens *tokens) {
  // edge cases
  if (tokens == NULL)
    return 0;

  char *argument = NULL;
  size_t tokensLength = tokens_get_length(tokens);
  for (size_t index = 0; index < tokensLength; index++) {
    // iterate through the command arguments and check if any of them is a pipe symbol
    argument = tokens_get_token(tokens, index);
    if (is_pipe_symbol(argument)) {
      return 1;
    }
  }
  return 0;
}

/**
 * Count the number of processes within the pipeline.
 * 
 * @param tokens The list of command tokens
 * 
 * @return int The number of processes in the pipeline. If tokens is:
 *             NULL => 0;
 *             [] => 0;
 *             [A] => 1;
 *             [A | ] => 1;
 *             [A | B] => 2;
 *             [A | B | ] => 2;
 *             ...
 */
int count_piped_processes(struct tokens *tokens) {
  // edge cases
  if (tokens == NULL)
    return 0;

  // edge case where tokens is empty
  size_t tokensLength = tokens_get_length(tokens);
  if (tokensLength == 0)
    return 0;

  // at least one process if tokens is not empty
  int numOfProcesses = 1;
  bool pipeEncountered = false;
  char *argument = NULL;
  for (size_t index = 0; index < tokensLength; index++) {
    argument = tokens_get_token(tokens, index);
    if (is_pipe_symbol(argument)) {
      pipeEncountered = true;
    } else if (pipeEncountered) {
      // only increment the process count if a program call follows a pipe symbol
      pipeEncountered = false;
      numOfProcesses++;
    }
  }
  return numOfProcesses;
}

/**
 * Free the list of file paths.
 * 
 * @param filePaths A list of file paths, end with a NULL pointer;
 *                  Both the list and the file path strings are dynamically-allocated
 * 
 * @return void
 */
void free_file_paths(char **filePaths) {
  // edge cases
  if (filePaths == NULL)
    return;
  
  // free each of the dynamically allocated file paths
  char **paths = filePaths;
  while ((*paths) != NULL) {
    free(*paths);
    paths++;
  }

  // free the list itself
  free(filePaths);
  return;
}

/**
 * Free the list of program names.
 * 
 * @param programNames A list of program names, end with a NULL pointer;
 *                     Only the list was dynamically-allocated, the program
 *                     name strings are not
 * 
 * @return void
 */
void free_program_names(char **programNames) {
  // edge cases
  if (programNames == NULL)
    return;
  
  // free the list of program names, not the program names per se
  free(programNames);
  return;
}

/**
 * Free the array of argument lists.
 * 
 * @param programArgLists An array of lists, end wtih a NULL pointer;
 *                        Only the array and its argument lists are dynamically-
 *                        allocated, the arguments are not
 * 
 * @return void
 */
void free_program_argument_lists(char ***programArgLists) {
  // edge cases
  if (programArgLists == NULL)
    return;
  
  // free each argument list, until a NULL pointer is encountered
  char ***argLists = programArgLists;
  while ((*argLists) != NULL) {
    free(*argLists);
    argLists++;
  }

  // free the argument list array
  free(programArgLists);
  return;
}

/**
 * Returns the full paths of the given executable files. Both the list and
 * each of the full paths are newly-allocated.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param fileNames Names of the executable file
 * 
 * @return char** The list of full paths of the executable files, or NULL
 *                if the full path of any one file is not found, i.e. the
 *                executable file does not exist
 */
char** resolve_executable_full_paths(const char **fileNames) {
  // edge cases
  if (fileNames == NULL)
    return NULL;
  
  // count the length of the file names array
  int length = 0;
  for (int index = 0; fileNames[index] != NULL; index++)
    length++;

  /*
   * allocate memory for the list of executable full paths; use calloc so
   * so that unitialized pointers are NULL and not random address
   */
  char **fileFullPaths = (char **) calloc((length + 1), sizeof(char *)); /** TODO: remember to free this */
  if (fileFullPaths == NULL) {
    perror("Failed to allocate memory: ");
    return NULL;
  }
  // the full paths list should end with a NULL pointer
  fileFullPaths[length] = NULL;

  // instantiate a local fileNamesPtr, because fileNames points to constant file names
  char *fileName = NULL, *fullPath = NULL, **fileNamePtr = (char **) fileNames;
  for (int index = 0; index < length; index++) {
    // resolve the full path of each of the file names in the list
    fileName = fileNamePtr[index];
    fullPath = resolve_executable_full_path(fileName);
    
    if (fullPath == NULL) {
      fprintf(stderr, "Failed to resolve file %s\n", fileName);
      free_file_paths(fileFullPaths);
      return NULL;
    } else {
      fileFullPaths[index] = fullPath;
    }
  }
  return fileFullPaths;
}

/**
 * Return the program names from the command tokens in a NULL terminated
 * array of strings. The strings are not newly-allocated, but the string
 * array is.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param tokens The list of command tokens
 * @param programNamesPtr The address of a list of program names
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed 
 */
int get_piped_program_names(struct tokens *tokens, char ***programNamesPtr) {
    // edge cases
    if (tokens == NULL || programNamesPtr == NULL) {
        *programNamesPtr = NULL;
        return -1;
    }

    // count the number of processes in the pipeline
    int numOfProcesses = count_piped_processes(tokens);
    if (numOfProcesses == 0) {
        *programNamesPtr = NULL;
        return 0;
    }
    
    /*
     * allocate memory for the list of program names; use calloc so that
     * unassigned locations will be interpreted as NULL pointers that signal
     * the end of traversal
     */
    char **programNames = NULL;
    programNames = (char **) calloc((numOfProcesses + 1), sizeof(char*));
    if (programNames == NULL) {
        perror("Failed to allocate memory: ");
        *programNamesPtr = NULL;
        return -1;
    }
    // the list should end with a NULL pointer
    programNames[numOfProcesses] = NULL;

    size_t tokensLength = tokens_get_length(tokens);
    char *argument = NULL, **tempProgramNames = programNames;
    bool pipeEncountered = true;
    for (size_t index = 0; index < tokensLength; index++){
        // iterate through the command tokens and extract all the program names
        argument = tokens_get_token(tokens, index);
        if (is_pipe_symbol(argument)){
            pipeEncountered = true;
        } else if (pipeEncountered) {
            // only extract the token if the previous token was a pipe symbol
            pipeEncountered = false;
            *tempProgramNames = argument;
            tempProgramNames++;
        }
    }

    *programNamesPtr = programNames;
    return 0;
}

/**
 * Return the program arguments from the command tokens in a NULL terminated
 * array of argument lists. The returned arguments are not newly-allocated,
 * but the argument list and the argument list array are.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param tokens The list of command tokens
 * @param programArgListsPtr The address of an array of argument lists
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_piped_program_arguments(struct tokens *tokens, char ****programArgListsPtr) {
  // edge cases
  if (tokens == NULL || programArgListsPtr == NULL) {
    *programArgListsPtr = NULL;
    return -1;
  }

  // count the number of processes in the pipeline
  int numOfProcesses = count_piped_processes(tokens);
  if (numOfProcesses == 0) {
    *programArgListsPtr = NULL;
    return 0;
  }

  /*
   * allocate memory to store the length of each argument list
   */
  int *argListLengths = (int *) malloc((numOfProcesses + 1) * sizeof(int)); /** TODO: remember to free this */
  if (argListLengths == NULL) {
    perror("Failed to allocate memory: ");
    *programArgListsPtr = NULL;
    return -1;
  }
  // the list of argument lengths should end with a -1 
  argListLengths[numOfProcesses] = -1;

  /*
   * record the length of each argument list
   */
  size_t tokensLength = tokens_get_length(tokens);
  for (int argListIndex = 0, startIndex = -1, finishIndex = 0; finishIndex < tokensLength; finishIndex++) {
    char *token = tokens_get_token(tokens, finishIndex);
    if (is_pipe_symbol(token)) {
      /*
       * if a pipe symbol is encountered, the previous argument list ended,
       * so update the argument list index and the start token index
       */
      argListIndex++;
      startIndex = finishIndex;
    } else {
      // otherwise, the argument list is growing
      argListLengths[argListIndex] = finishIndex - startIndex;
    }
  }

  /*
   * allocate memory for the array of argument lists; use calloc so that
   * unassigned locations will be interpreted as NULL pointers that signal
   * the end of traversal
   */
  char ***argLists = (char ***) calloc((numOfProcesses + 1), sizeof(char **)); /** TODO: remember to free this */
  if (argLists == NULL) {
    perror("Failed to allocate memory: ");
    free(argListLengths);
    *programArgListsPtr = NULL;
    return -1;
  }
  // the array of argument lists should end with a NULL
  argLists[numOfProcesses] = NULL;

  /*
   * allocate memory for each argument list
   */
  for (int index = 0; index < numOfProcesses; index++) {
    int argListLength = argListLengths[index];

    char **argList = (char **) malloc((argListLength + 1) * (sizeof(char *))); /** TODO: remember to free this */
    if (argList == NULL) {
      perror("Failed to allocate memory: ");
      free_program_argument_lists(argLists); // free each argument list first
      free(argLists); // then free the argument list array
      free(argListLengths);
      *programArgListsPtr = NULL;
      return -1;
    }
    // each argument list should end with a NULL
    argList[argListLength] = NULL;

    argLists[index] = argList;
  }
  free(argListLengths);

  /*
   * extract all tokens into argument lists
   */
  for (int tokenIndex = 0, argListIndex = 0, argIndex = 0; tokenIndex < tokensLength; tokenIndex++) {
    char **argList = argLists[argListIndex];
    char *token = tokens_get_token(tokens, tokenIndex);

    if (is_pipe_symbol(token)) {
      /*
       * if a pipe symbol is encountered, the previous argument list ended,
       * so update the argument index and argument list index
       */
      argIndex = 0;
      argListIndex++;
    } else {
      // otherwise, extract the token into the argument list
      argList[argIndex++] = token;
    }
  }

  *programArgListsPtr = argLists;
  return 0;
}

/**
 * Parse the command tokens into an array of program names and an array of
 * argument lists. The returned program names and arguments are not
 * newly-allocated, but the program name array, argument array, and argument
 * lists are.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param tokens The list of command tokens
 * @param programNamesPtr The address of the program name array
 * @param argListsPtr The address of the argument lists array
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int parse_piping_tokens(struct tokens *tokens, char ***programNamesPtr, char ****argListsPtr) {
  // edge cases
  if (tokens == NULL || programNamesPtr == NULL || argListsPtr == NULL) {
    *programNamesPtr = NULL;
    *argListsPtr = NULL;
    return -1;
  }

  /* 
   * parse and extract the piped program's names and arguments
   */
  int result = 0;
  result = get_piped_program_names(tokens, programNamesPtr);
  if (result != 0) {
    *programNamesPtr = NULL;
    *argListsPtr = NULL;
    return -1;
  }

  result = get_piped_program_arguments(tokens, argListsPtr);
  if (result != 0) {
    free_program_names(*programNamesPtr);
    *programNamesPtr = NULL;
    *argListsPtr = NULL;
    return -1;
  }

  return 0;
}

/**
 * Executes a program, with default input and output file descriptors
 * redirected to the given file descriptors. The subprocess will be
 * suspended until it receives a SIG_CONT.
 * 
 * This function should only be called by a forked child process, as it
 * switches the caller program image into the given program image, and
 * does not return.
 * 
 * @param programFullPath The full path of the executable file
 * @param programArgList A list of argument strings
 * @param inputFileNum The number of the file descriptor replacing default input
 * @param outputFileNum The number of the file descriptor replacing default output
 * 
 * @return void
 */
void execute_with_redirection(char *programFullPath, char **programArgList, int inputFileNum, int outputFileNum) {
  /*
   * edge cases; exits the process directly, because this function does not
   * return. Does not free the heap memory; let garbage collector handle it instead
   */
  if (programFullPath == NULL || programArgList == NULL) {
    fprintf(stderr, "Failed to execute program %s\n", programFullPath);
    exit(1);
  }

  /*
   * if the input/output file descriptor number are not the default ones,
   * overwrite the standard input/output file descriptors
   */
  int redirectionResult = -1;
  if (inputFileNum != STDIN_FILENO) {
    redirectionResult = dup2(inputFileNum, STDIN_FILENO);
    if (redirectionResult == -1) {
      fprintf(stderr, "Failed to overwrite input/output\n");
      perror("Input/output redirection failed: ");
      exit(1);
    }
    close(inputFileNum);
  }
  if (outputFileNum != STDOUT_FILENO) {
    redirectionResult = dup2(outputFileNum, STDOUT_FILENO);
    if (redirectionResult == -1) {
      fprintf(stderr, "Failed to overwrite input/output\n");
      perror("Input/output redirection failed: ");
      exit(1);
    }
    close(outputFileNum);
  }

  /*
   * execute the given program ith I/O redirection; but first, suspend this
   * process so that the shell process can assign it to another process group
   */
  kill(getpid(), SIGSTOP);
  reset_ignored_signals();
  execv(programFullPath, (char* const *) programArgList);
}

/**
 * Execute the given programs with the given arguments, accounting for the
 * piping options. The subprocesses will be suspended until they receive a
 * SIG_CONT.
 * 
 * Takes the ownership of argument list and frees it.
 * 
 * @param programNames The array of program names
 * @param programArgLists The array of program argument lists; will be freed by this function
 * 
 * @return pid_t* Returns the list of subprocess IDs
 */
pid_t *execute_piped_program(const char **programNames, const char ***programArgLists) {
  // edge cases
  if (programNames == NULL || programArgLists == NULL) {
    return NULL;
  }

  // resolve the full paths of the program executables
  char **programFullPaths = resolve_executable_full_paths(programNames); /** TODO: remember to free this */
  if (programFullPaths == NULL) {
    fprintf(stderr, "No such executable program\n");
    free_program_names((char **) programNames);
    free_program_argument_lists((char ***) programArgLists);
    return NULL;
  }

  // count the number of programs to call
  int numOfPrograms = 0;
  for (int index = 0; programFullPaths[index] != NULL; index++)
    numOfPrograms++;

  // allocate memory for the program process IDs
  pid_t *processIDs = (pid_t *) malloc((numOfPrograms + 1) * sizeof(pid_t));  /** TODO: remember to free this */
  if (processIDs == NULL) {
    perror("Failed to allocate memory: ");
    free_file_paths(programFullPaths);
    free_program_names((char **) programNames);
    free_program_argument_lists((char ***) programArgLists);
    return NULL;
  }
  // assign -1 to all unused locations to signal a stop to traversal
  for (int index = 0; index < numOfPrograms; index++) {
    processIDs[index] = -1;
  }

  /*
   * execute the programs as a sequence of child processes, with adjacent
   * programs sharing a pipe
   */
  int pipeFileNums[2], inputFileNum = -1, outputFileNum = -1, tempFileNum = STDIN_FILENO;
  for (int index = 0; index < numOfPrograms; index++) {
    // prepare the input/output file descriptors for this program
    if (index < (numOfPrograms - 1)) {
      /*
       * if this is not the last program, then there's a successor reading
       * from this program, and we would need a pipe for these two adjacent
       * programs
       */
      pipe(pipeFileNums);

      // tempFileNum is the either STDIN_FILENO or the read end of the last pipe
      inputFileNum = tempFileNum;
      outputFileNum = pipeFileNums[1];
      tempFileNum = pipeFileNums[0];
    } else {
      // this is the last program, so the output should be STDOUT_FILENO
      inputFileNum = tempFileNum;
      outputFileNum = STDOUT_FILENO;
      tempFileNum = -1;
    }

    // execute the program as a child process
    pid_t processID = fork();
    if (processID == -1) {
      perror("Failed to create new process: ");
      free(processIDs);
      free_file_paths(programFullPaths);
      free_program_names((char **) programNames);
      free_program_argument_lists((char ***) programArgLists);
      /*
       * if we failed to fork a process in the pipeline, then none of the
       * successor processes would get correct input. So we abort the rest of
       * the processes and wait for the predecessor processes to finish
       */
      break;
    } else if (processID == 0) {
      /*
       * this is the child process, and it should be run with stdin/stdout
       * redirection. The child process should exit via the new process image,
       * so there's no need to return
       */
      execute_with_redirection(programFullPaths[index], (char **) programArgLists[index], inputFileNum, outputFileNum);
    }

    /*
     * this is the parent process, which iterates to execute each program in the pipeline
     */
    // append the new child process ID to the list of executed processes
    processIDs[index] = processID;
    // close the input/output file descriptors that were opened for the child process
    if (inputFileNum != STDIN_FILENO)
      close(inputFileNum);
    if (outputFileNum != STDOUT_FILENO)
      close(outputFileNum);
  }

  // after creating all the subprocesses, return the list of process IDs
  free_file_paths(programFullPaths);
  free_program_names((char **) programNames);
  free_program_argument_lists((char ***) programArgLists);
  return processIDs;
}