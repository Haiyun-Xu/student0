/**
 * This module contains built-in commands for the shell.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "shell_command.h"


// built-in command description struct
struct cmd_description {
  command_t *fun;
  char *cmd;
  char *doc;
};

struct cmd_table {
  int numOfCommands;
  // the capacity of commandTable must be larger than the numOfCommands
  struct cmd_description commandTable[10];
};

// built-in command description table
struct cmd_table shellCommands = {
  6,
  {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "print the current working directory path"},
    {cmd_cd, "cd", "change the current working directory"},
    {cmd_fg, "fg", "move a subprocess to the foreground"},
    {cmd_bg, "bg", "move a subprocess to the background"}
  }
};

/**
 * A built-in command of the shell. Brings up the help menu and prints a helpful description for the given command
 * 
 * @return int Returns 0 if successful, pr -1 otherwise
 */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int index = 0; index < shellCommands.numOfCommands; index++)
    printf("%s - %s\n", shellCommands.commandTable[index].cmd, shellCommands.commandTable[index].doc);
  
  return 0;
}

/**
 * A built-in command of the shell. Exits this shell process.
 * 
 * @return void
 */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

/**
 * A built-in command of the shell. Prints the current working directory in the file system.
 * 
 * @return int Returns 0 if successful, pr -1 otherwise
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
    bytesWritten += write(SHELL_OUTPUT_FILE_NUM, (void *)(cwdPathName + bytesWritten), (pathNameLength - bytesWritten));
  write(SHELL_OUTPUT_FILE_NUM, (void *)&newline, 1);

  free(cwdPathName);
  return 0;
}

/**
 * A built-in command of the shell. Changes the current working directory in the file system.
 * 
 * @param tokens The list of command arguments
 * 
 * @return int Returns 0 if successful, pr -1 otherwise
 */
int cmd_cd(struct tokens *tokens) {
  // edge cases
  if (tokens == NULL) {
    return -1;
  }

  // the desired working directory should be the second token
  char *pathName = tokens_get_token(tokens, 1);

  if (chdir(pathName)) {
    perror("Failed to change current working directory: ");
    return -1;
  }
  
  return 0;
}

/**
 * A built-in command of the shell. Moves a given process to the foreground.
 * 
 * @param tokens The list of command arguments
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int cmd_fg(struct tokens *tokens) {
  // edge cases
  if (tokens == NULL) {
    return -1;
  }

  pid_t processID = -1;
  if (tokens_get_length(tokens) > 1) {
    /*
     * if a second argument is given, it might be a subprocess ID.
     * Validate that it is an integer.
     */
    char *argument = tokens_get_token(tokens, 1);
    int argLength = strlen(argument);
    for (int index = 0; index < argLength; index++) {
      if (!isdigit(argument[index])) {
        fprintf(stderr, "First argument must be a process ID\n");
        return -1;
      }
    }
    processID = atoi(argument);
  } else {
    /* 
     * if there's no second argument, move the most recent subprocess to
     * the foreground
     */
    processID = get_latest_process();
  }

  /*
   * first set the subprocess's process group as the foreground, then
   * send a resume signal to the process, in case it was suspended
   */
  int result = set_foreground_process(SHELL_INPUT_FILE_NUM, processID);
  if (result == -1) {
    fprintf(stderr, "Failed to move process to foreground\n");
    return -1;
  }

  start_process(processID);
  return 0;
}

/**
 * A built-in command of the shell. Moves a given process to the background.
 * 
 * @param tokens The list of command arguments
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int cmd_bg(struct tokens *tokens) {
  // edge cases
  if (tokens == NULL) {
    return -1;
  }

  pid_t processID = -1;
  if (tokens_get_length(tokens) > 1) {
    /*
     * if a second argument is given, it might be a subprocess ID.
     * Validate that it is an integer.
     */
    char *argument = tokens_get_token(tokens, 1);
    int argLength = strlen(argument);
    for (int index = 0; index < argLength; index++) {
      if (!isdigit(argument[index])) {
        fprintf(stderr, "First argument must be a process ID\n");
        return -1;
      }
    }
    processID = atoi(argument);
  } else {
    /* 
     * if there's no second argument, move the most recent subprocess to
     * the foreground
     */
    processID = get_latest_process();
  }

  /*
   * since the shell process was in the foreground, the given subprocess was
   * definitely in the background. And since this command sets no new process
   * group to the foreground, the shell will continue to be the foreground,
   * and we only have to make sure that the given background process was resumed
   */
  start_process(processID);
  return 0;
}

/**
 * Look up and return the built-in command function.
 * 
 * @param command The command string
 * 
 * @return command_t* The command function, or NULL otherwise
 */
command_t *shell_command_lookup(char* command) {
  // edge cases
  if (command == NULL) {
    return NULL;
  }

  for (unsigned int index = 0; index < shellCommands.numOfCommands; index++) {
    if (strcmp(command, shellCommands.commandTable[index].cmd) == 0) {
      return shellCommands.commandTable[index].fun;
    }
  }
  return NULL;
}