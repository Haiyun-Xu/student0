/**
 * This module contains a simple shell program that supports built-in commands,
 * executable programs, and job control.
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
#include <unistd.h>

#include "shell_command.h"
#include "shell_config.h"
#include "shell_signal.h"
#include "tokenizer.h"
#include "program.h"
#include "program_piping.h"
#include "program_redirection.h"
#include "process_management.h"

/**
 * Intialize the shell process. This includes setting the shell process group
 * as the foreground, and registering the signal handlers.
 * 
 * @return int Return 0 if successful, or -1 otherwise
 */
int initialize_shell() {
  // The shell should use stdin/stdout as the I/O file descriptors.
  SHELL_INPUT_FILE_NUM = STDIN_FILENO;
  SHELL_OUTPUT_FILE_NUM = STDOUT_FILENO;

  /*
   * Check if the shell is running interactively, i.e. if the input file
   * descriptor is an open, terminal descriptor
   */
  shellIsInteractive = isatty(SHELL_INPUT_FILE_NUM);

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
      int foregroundProcessGroupID = tcgetpgrp(SHELL_INPUT_FILE_NUM);
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
    tcsetpgrp(SHELL_INPUT_FILE_NUM, shellProcessGroupID);

    // Save the current termios to a variable, so it can be restored later.
    tcgetattr(SHELL_INPUT_FILE_NUM, &shell_tmodes);
  }

  /*
   * once the shell is in the teletype's foreground process group, its signal
   * handlers can be configured. These custom signal handlers help the shell
   * process ignore insignificant signals and maintain the subprocesses list,
   * which we also initialize here
   */
  int result = 0;
  result = register_shell_signal_handlers();
  if (result == -1) {
    return -1;
  }

  result = initialize_process_list();
  if (result == -1) {
    return -1;
  }

  return 0;
}

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
  bool backgroundExecution = false;
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
    command_t *shellCommand = NULL;
    if ((shellCommand = shell_command_lookup(get_program_name(tokens))) != NULL) {
      return shellCommand(tokens);
    } else {
      // extract the program name and arguments
      char *programName = NULL, **argList = NULL;
      int result = parse_tokens(tokens, &programName, &argList);
      if (result != 0) {
        fprintf(stderr, "Failed to parse program name and arguments from command line\n");
        return -1;
      }

      /*
       * check whether the program should be executed in the background;
       * if so, remove the background-execution flag from the argument list
       */
      backgroundExecution = should_exec_in_background(tokens);
      if (backgroundExecution) {
        int lastArgIndex = tokens_get_length(tokens) - 1;
        argList[lastArgIndex] = NULL;
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
  int status = manage_shell_subprocesses(subprocessIDs, SHELL_INPUT_FILE_NUM, SHELL_OUTPUT_FILE_NUM, backgroundExecution);
  free(subprocessIDs);
  return status;
}

/**
 * The shell program.
 */
int main(unused int argc, unused char *argv[]) {
  int status = initialize_shell();
  if (status == -1) {
    fprintf(stderr, "Failed to initialize the shell\n");
    exit(1);
  }

  static char line[COMMAND_LINE_LENGTH];
  struct tokens *tokens = NULL;
  int lineNumber = 0;

  // the loop should never terminate, unless the exit() command is executed
  while (true) {
    // only print shell prompts when standard input is not a tty
    if (shellIsInteractive) {
      fprintf(stdout, "%d: ", lineNumber++);
      fflush(stdout);
    }

    /*
     * The ideal behavior of the shell should be as follows:
     * 1) the shell disables SA_RESTART when registering the signal handlers,
     * such that signal-interrupted system calls are not restarted;
     * 2) whenever the shell is blocked and reading user input, if user types
     * a special signal-generating character, the terminal will send that
     * signal to the shell;
     * 3) the execution jumps from the system call to the signal handler,
     * which ignores the signal and returns the control back to the system
     * call;
     * 4) since SA_RESTART was disabled, the system call should just return to
     * user code, WITHOUT restarting the system call;
     * 5) the shell process should check the line returned, and move onto the
     * next line if the line was incomplete or if the errno was set to EINTR;
     * 
     * Note that, somehow we must implement our own signal ignorer instead of
     * using SIG_IGN, or else the read syscall always gets retried.
     * 
     * The only problem with this method is that, when fgets() returns due
     * to interruption, its line buffer still contains the command from last
     * run. We deal with this by overwriting the line buffer with null chars
     * everytime the line is parsed into tokens.
     * 
     * Here we read the command line, split the command program call into
     * arguments, execute the program call, and clean up the arguments
     */
    read(SHELL_INPUT_FILE_NUM, line, COMMAND_LINE_LENGTH);
    if (errno == EINTR) {
      write(SHELL_OUTPUT_FILE_NUM, "\n", 2);
    }
    tokens = tokenize(line);
    clean_string(line, COMMAND_LINE_LENGTH);
    execute_commandline(tokens);
    tokens_destroy(tokens);
  }
  // this should never be reached
  return 0;
}
