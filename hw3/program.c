/**
 * This module contains helper functions for non built-in programs.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "program.h"

/**
 * Return the program name.
 * 
 * @param tokens The list of command tokens
 * 
 * @return char* The program name
 */
char *get_program_name(struct tokens *tokens) {
  if (tokens == NULL)
    return NULL;

  return tokens_get_token(tokens, 0);
}

/**
 * Get the program arguments from the command.
 * The program arguments are not newly-allocated, but the pointer array to the program arguments is.
 * Caller should own the heap memory and free it.
 * 
 * @param tokens The list of command arguments
 * @param argListPtr The address of an argument list
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_program_arguments(struct tokens *tokens, char ***argListPtr) {
  // edge cases
  if (tokens == NULL || argListPtr == NULL) {
    *argListPtr = NULL;
    return -1;
  }
  
  // allocate memory for the arguments list
  size_t argListLength = tokens_get_length(tokens);
  char **argList = (char **) malloc((argListLength + 1) * sizeof(char*));
  if (argList == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    *argList = NULL;
    return -1;
  }
  // the argument list should end with a NULL pointer
  argList[argListLength] = NULL;

  // extract all the command tokens into the argument list
  for (size_t index = 0; index < argListLength; index++) {
    argList[index] = tokens_get_token(tokens, index);
  }
  *argListPtr = argList;

  return 0;
}

/**
 * Parse the command tokens into a program name and an argument list. The
 * returned program name and arguments are not newly-allocated, but the
 * argument list is.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param tokens The command tokens
 * @param programNamePtr The address of the program name
 * @param argListPtr The address of the argument list
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int parse_tokens(struct tokens *tokens, char **programNamePtr, char ***argListPtr) {
  // edge cases
  if (tokens == NULL || programNamePtr == NULL || argListPtr == NULL) {
    *programNamePtr = NULL;
    *argListPtr = NULL;
    return -1;
  }
 
  // parse and extract the program's name and arguments
  char *programName = get_program_name(tokens);
  if (programName == NULL) {
    *programNamePtr = NULL;
    *argListPtr = NULL;
    return -1;
  }
  *programNamePtr = programName;

  int result = get_program_arguments(tokens, argListPtr);
  if (result != 0) {
    *programNamePtr = NULL;
    *argListPtr = NULL;
    return -1;
  }

  return 0;
}

/**
 * Execute the given program with the given arguments. The subprocess will
 * be suspended until it receives a SIG_CONT.
 * 
 * @param programName The program name
 * @param programArgList The list of program arguments; will be freed by this function
 * 
 * @return pid_t* Returns a list of subprocess IDs
 */
pid_t *execute_program(const char *programName, char **programArgList) {
  // edge cases
  if (programName == NULL || programArgList == NULL) {
    return NULL;
  }

  char *programFullPath = resolve_executable_full_path(programName); /** TODO: remember to free this */
  if (programFullPath == NULL) {
    fprintf(stderr, "No such executable program\n");
    free(programArgList);
    return NULL;
  }

  // allocate memory for the process ID list
  pid_t *processIDs = (pid_t *) malloc((1 + 1) * sizeof(pid_t)); /** TODO: remember to free this */
  if (processIDs == NULL) {
    perror("Failed to allocate memory: ");
    free(programFullPath);
    free(programArgList);
    return NULL;
  }
  // the process ID list should end with a -1
  processIDs[1] = -1;

  // execute the program as a child process
  pid_t processID = fork();
  if (processID == -1) {
    perror("Failed to create new process: ");
    free(processIDs);
    free(programFullPath);
    free(programArgList);
    return NULL;
  } else if (processID == 0) {
    /*
     * this is the child process, so execute the program; but first, suspend
     * this process so that the shell process can assign it to another process
     * group. The child process should exit via the new process image, so
     * there's no need to return
     */
    kill(getpid(), SIGSTOP);
    execv(programFullPath, (char *const *) programArgList);
  }

  // this is the parent process, which returns the list of child process IDs
  processIDs[0] = processID;
  free(programFullPath);
  free(programArgList);
  return processIDs;
}