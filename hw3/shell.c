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
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"
#include "program_helpers.h"
#include "program_redirection.h"



/**
 * Configurations of the shell
 */
// Whether the shell is connected to an actual terminal or not
bool shell_is_interactive;

// File descriptor to use as the shell's input and output file
int shell_input;
int shell_output;

// Terminal mode settings for the shell
struct termios shell_tmodes;

// Process group id for the shell
pid_t shell_pgid;

/**
 * Intialization procedures for this shell.
 */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_input = STDIN_FILENO;
  shell_output = STDOUT_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_input);

  if (shell_is_interactive) {
    /**
     * If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT.
     * 
     * We detect whether the shell is in the foreground by checking if the process owning stadin is
     * the shell process (compare the process group ids). If not, we send to all processes within the
     * group a SIGTTIN signal (stop terminal input for background process).
     */
    while (tcgetpgrp(shell_input) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_input, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_input, &shell_tmodes);
  }
}



/**
 * Built-in commands of this shell
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
    bytesWritten += write(shell_output, (void *)(cwdPathName + bytesWritten), (pathNameLength - bytesWritten));
  write(shell_output, (void *)&newline, 1);

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
   * its syntax: input/output redirect, piping, and command/program call
   */
  if (contains_redirection(tokens)) {
    // extract the program call tokens
    struct tokens *programCallTokens = NULL;
    get_redirection_program_call(tokens, &programCallTokens); /** TODO: remember to free this */
    
    // parse the tokens to get the program full path and argument list
    char *programFullPath = NULL, **programArgList = NULL;
    interpret_command_arguments(programCallTokens, &programFullPath, &programArgList); /** TODO: remember to free this */
    if (programFullPath == NULL) {
      fprintf(stderr, "Failed to find program executable\n");
      tokens_destroy(programCallTokens);
      return -1;
    }

    // extract the program redirection file name
    char *redirectionFileName = NULL;
    int redirectionType = get_redirection_file_name(tokens, &redirectionFileName);

    // run the program executable with input/output redirection
    int exitStatus = execute_program_redirected(programFullPath, programArgList, redirectionType, redirectionFileName);
    
    tokens_destroy(programCallTokens);
    free(programFullPath);
    free(programArgList);
    return exitStatus;
  }

  /*
   * no special syntax, so either find and run the requested built-in
   * command, or run an external program;
   */
  int commandIndex = lookup(tokens_get_token(tokens, 0));
  if (commandIndex >= 0) {
    return cmd_table[commandIndex].fun(tokens);
  } else {
    char *programFullPath = NULL, **programArgList = NULL;
    interpret_command_arguments(tokens, &programFullPath, &programArgList); /** TODO: remember to free this */
    if (programFullPath == NULL) {
      fprintf(stderr, "Failed to find program executable\n");
      return -1;
    }

    // run the program executable
    int exitStatus = execute_program(programFullPath, programArgList);

    free(programFullPath);
    free(programArgList);
    return exitStatus;
  }
}

/**
 * The shell program.
 */
int main(unused int argc, unused char *argv[]) {
  init_shell();

  static char line[4096];
  struct tokens *tokens = NULL;
  int line_num = 0;

  // the loop should never terminate, unless the exit() command is executed
  while (true) {
    // only print shell prompts when standard input is not a tty
    if (shell_is_interactive)
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
