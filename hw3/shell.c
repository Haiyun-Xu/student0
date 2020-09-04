/**
 * This is a simple shell program that supports built-in commands as well as other programs on the computer.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"
#include "shell_signal.h"
#include "program.h"
#include "program_piping.h"
#include "program_redirection.h"
#include "process_management.h"

/**
 * Configurations of the shell
 */
// Whether the shell is connected to an actual terminal or not
bool shellIsInteractive;

// File descriptor to use as the shell's input and output file
int shellInputFileNum;
int shellOutputFileNum;

// Terminal mode settings for the shell
struct termios shell_tmodes;

// the shell's process group ID, and the terminal's foreground process group ID
pid_t shellProcessGroupID;
pid_t terminalForegroundProcessGroupID;

/**
 * Intialize the shell process. This includes setting the shell process group
 * as the foreground, and setting up the signal handlers.
 * 
 * @return void
 */
void initialize_shell() {
  // The shell should use stdin/stdout as the I/O file descriptors.
  shellInputFileNum = STDIN_FILENO;
  shellOutputFileNum = STDOUT_FILENO;

  /*
   * Check if the shell is running interactively, i.e. if the input file
   * descriptor is an open, terminal descriptor
   */
  shellIsInteractive = isatty(shellInputFileNum);

  /*
   * If the shell is using the terminal as its input file descriptor, then
   * the shell process must be in the foreground process group of the terminal
   * descriptor; otherwise it will not be able to read from the terminal
   * and will be stopped by the SIGTTIN signal 
   */
  if (shellIsInteractive) {
    /*
     * If the shell process is currently not in the foreground process group,
     * it will not be able to use the terminal, and so must pause its
     * execution and wait for its turn.
     * 
     * We detect whether the shell is in the foreground by checking the ID
     * of the foreground process group associated with the terminal. And we
     * pause the shell's execution by putting its process group into the
     * background via a SIGTTIN signal.
     * 
     * When the shell process group gets promoted to the foreground process
     * group, it will receive a SIGCONT and can continue to execute
     */
    do {
      shellProcessGroupID = getpgrp();
      int foregroundProcessGroupID = tcgetpgrp(shellInputFileNum);
      if (shellProcessGroupID != foregroundProcessGroupID) {
        // suspends all processes in the shell's process group
        kill(-shellProcessGroupID, SIGTTIN);
      } else {
        break;
      }
    } while (true);

    /*
     * the shell's process group should now be the foreground process group
     * of the terminal, so make the shell process the foreground process?
     */
    shellProcessGroupID = getpid();
    tcsetpgrp(shellInputFileNum, shellProcessGroupID);

    // Save the current termios to a variable, so it can be restored later.
    tcgetattr(shellInputFileNum, &shell_tmodes);
  }

  /**
   * once the shell is in the teletype's foreground process group, its signal
   * handlers can be configured.
   * 
   * for security and stability reasons, the kernel will not use the
   * sigaction struct stored in the user memory. Instead, it should copy
   * the content of the user memory sigaction struct into a kernel memory 
   * signaction struct. In that case, the user memory sigaction struct is
   * never again referrenced after signaction() returns, so we can use a
   * singleton sigaction struct as the argument when calling sigaction()
   * with different signals.
   */
  struct sigaction signalHandler;
  signalHandler.sa_handler = ignore_signal;

  const struct signal_group *ignoredSignals = get_ignored_signals();
  for (int index = 0; index < ignoredSignals->numOfSignals; index++) {
    sigaction(ignoredSignals->signals[index], &signalHandler, NULL);
  }
  return;
}



/**
 * Built-in commands of the shell
 */
// Convenience macro to silence compiler warnings about unused function parameters.
#define unused __attribute__((unused))

// Built-in command functions take token array (see parse.h) and return int
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

int cmd_help(unused struct tokens *tokens);
int cmd_exit(unused struct tokens *tokens);
int cmd_pwd(unused struct tokens *tokens);
int cmd_cd(struct tokens *tokens);

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_pwd, "pwd", "print the current working directory path"},
  {cmd_cd, "cd", "change the current working directory"},
};

/**
 * A built-in command of the shell. Brings up the help menu and prints a helpful description for the given command
 * 
 * @return int Returns 0 if command completed successfully, or an error code
 */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/**
 * A built-in command of the shell. Exits this shell process.
 * 
 * @return None Should exit the shell process directly
 */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

/**
 * A built-in command of the shell. Prints the current working directory in the file system.
 * 
 * @return int Returns 0 if command completed successfully, or -1 if failed
 */
int cmd_pwd(unused struct tokens *tokens) {
  /*
   * setting the buffer to NULL and the buffer size to 0 will cause getcwd()
   * to dynamically allocate a buffer as big as neccessary
   */
  char *cwdPathName = getcwd(NULL, 0);
  if (cwdPathName == NULL) {
    perror("Failed to get current working directory: ");
    return -1;
  }
  
  // print the current working directory to standard output
  size_t bytesWritten = 0, pathNameLength = strlen(cwdPathName);
  char newline = '\n';
  while (bytesWritten < pathNameLength)
    bytesWritten += write(shellOutputFileNum, (void *)(cwdPathName + bytesWritten), (pathNameLength - bytesWritten));
  write(shellOutputFileNum, (void *)&newline, 1);

  free(cwdPathName);
  return 0;
}

/**
 * A built-in command of the shell. Changes the current working directory in the file system.
 * 
 * @param tokens The list of command arguments
 * 
 * @return int Returns 0 if command completed successfully, or -1 if failed
 */
int cmd_cd(struct tokens *tokens) {
  // the desired working directory should be the second token
  char *pathName = tokens_get_token(tokens, 1);

  if (chdir(pathName)) {
    perror("Failed to change current working directory: ");
    return -1;
  }
  
  return 0;
}

/**
 * Look up and return the built-in command index if it exists. Otherwise return -1.
 * 
 * @param cmd Pointer to a command string
 * 
 * @return int Index of the command in the command table
 */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}


/**
 * Main functional components of the shell
 */
/**
 * Interpret and run the command line arguments.
 * 
 * @param tokens The list of command arguments
 * 
 * @return int Returns 0 if the command line ran successfully, or -1 otherwise
 */
int execute_commandline(struct tokens *tokens) {
  // edge cases
  if (tokens == NULL)
    return -1;

  /*
   * check the command line syntax, and execute it in different mode based on
   * its syntax: input/output redirect, piping, and command/program call.
   */
  int syntax = 0;
  pid_t *subprocessIDs = NULL;
  if (is_tokens_empty(tokens)) {
    /*
     * if the user sent a signal through the terminal, or if the command
     * starts with a newline character, the command tokens will be empty,
     * and the shell should return right away
     */
    return 0;

  } else if ((syntax = contains_redirection(tokens))) {
    // extract the program name, program arguments, and redirection file name
    char *programName = NULL, **argList = NULL, *fileName = NULL;
    int result = parse_redirection_tokens(tokens, &programName, &argList, &fileName);
    if (result != 0) {
      fprintf(stderr, "Failed to parse program name and arguments from command line\n");
      return -1;
    }
    // execute the redirected program
    subprocessIDs = execute_redirected_program(programName, argList, syntax, fileName); /** TODO: remember to free this */

  } else if (contains_piping(tokens)) {
    // extract the program names amd program argument lists
    char **programNames = NULL, ***argLists = NULL;
    int result = parse_piping_tokens(tokens, &programNames, &argLists);
    if (result != 0) {
      fprintf(stderr, "Failed to parse program name and arguments from command line\n");
      return -1;
    }
    // execute the piped program
    subprocessIDs = execute_piped_program((const char **) programNames, (const char ***) argLists); /** TODO: remember to free this */

  } else {
    /*
     * if the command line has no special syntax, then either find and run the
     * built-in command, or run the program;
     */
    int commandIndex = lookup(get_program_name(tokens));
    if (commandIndex >= 0) {
      return cmd_table[commandIndex].fun(tokens);
    } else {
      // extract the program name and arguments
      char *programName = NULL, **argList = NULL;
      int result = parse_tokens(tokens, &programName, &argList);
      if (result != 0) {
        fprintf(stderr, "Failed to parse program name and arguments from command line\n");
        return -1;
      }
      // execute the program
      subprocessIDs = execute_program(programName, argList); /** TODO: remember to free this */
    }
  }

  if (subprocessIDs == NULL) {
    fprintf(stderr, "Failed to execute\n");
    return -1;
  }
  
  // wait till the subprocesses have completed
  int status = manage_shell_subprocesses(subprocessIDs, shellInputFileNum, shellOutputFileNum);
  free(subprocessIDs);
  return status;
}

/**
 * The shell program.
 */
int main(unused int argc, unused char *argv[]) {
  initialize_shell();

  static char line[4096];
  struct tokens *tokens = NULL;
  int line_num = 0;

  // the loop should never terminate, unless the exit() command is executed
  while (true) {
    // only print shell prompts when standard input is not a tty
    if (shellIsInteractive)
      fprintf(stdout, "%d: ", line_num++);

    // reads the command line, split it into arguments, then run it
    fgets(line, 4096, stdin);
    tokens = tokenize(line);
    execute_commandline(tokens);

    // clean up memory
    tokens_destroy(tokens);
  }
  // this should never be reached
  return 0;
}
