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
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor to use as the shell's input and output file */
int shell_input;
int shell_output;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

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
 * Convert the command tokens into program name and arguments.
 * 
 * @param programName Address of a pointer to string
 * @param argList Address of a pointer to a list of strings
 * @param tokens The tokens struct containing all the command parts
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int parse_program_arguments(char **programName, char ***argList, struct tokens * tokens) {
  if (programName == NULL|| argList == NULL|| tokens == NULL) {
    return -1;
  }

  // extract the name of the program
  *programName = tokens_get_token(tokens, 0);

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
 * Execute non-built-in command by calling other programs, assuming that the
 * full path of the program is provided.
 * 
 * The new program is executed as a new process.
 */
int exec_program(struct tokens *tokens) {
  // extract the program name and all of its arguments
  char *programName, **argList;
  parse_program_arguments(&programName, &argList, tokens);

  // fork the current process
  pid_t processID = fork();
  if (processID == -1) {
    perror("Failed to create new process: ");
    return -1;
  } else if (processID == 0) {
    // this is the child process, should exit via code in the new process image
    execv(programName, argList);
  }

  // this is the parent process
  int terminatedProcessID = waitpid(processID, NULL, 0); // wc program doesn't take the argument and so hangs to wait for stdin 
  if (terminatedProcessID == -1) {
    perror("Failed to wait for child process to terminate: ");
    return -1;
  }
  return 0;
}

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

int main(unused int argc, unused char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      exec_program(tokens);
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
