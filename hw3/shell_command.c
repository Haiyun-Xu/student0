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
  7,
  {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "print the current working directory path"},
    {cmd_cd, "cd", "change the current working directory"},
    {cmd_wait, "wait", "wait for all the background processes to exit"},
    {cmd_fg, "fg", "move a subprocess to the foreground"},
    {cmd_bg, "bg", "move a subprocess to the background"}
  }
};

/**
 * A built-in command of the shell. Brings up the help menu and prints a helpful description for the given command
 * 
 * @return int Returns 0 if successful, or -1 otherwise
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
  destroy_process_list();
  exit(0);
}

/**
 * A built-in command of the shell. Prints the current working directory in the file system.
 * 
 * @return int Returns 0 if successful, or -1 otherwise
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
 * @return int Returns 0 if successful, or -1 otherwise
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
 * A built-in command of the shell. Wait untill all of the shell's background
 * processes exited.
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int cmd_wait(struct tokens *tokens) {
  /*
   * since the shell process gets the `wait` command from the terminal,
   * it must be in the foreground, and so all processes in the process list
   * are in the background. Therefore, traverse through the process list and
   * wait for each of the background processes
   */
  struct process_node *processNode = get_latest_process(), *exitedNode = NULL;

  while (processNode != NULL) {
    while (true) {
      int processStatus = wait_till_pause_or_exit(processNode->processID);
      /*
       * break the loop if the waiting failed or the background process exited;
       * otherwise, continue to wait for this process
       */
      if (processStatus == -1 || WIFEXITED(processStatus)) {
        exitedNode = processNode;
        break;
      }
    }

    // get the next process node before removing the exited process node
    processNode = get_next_process(processNode);
    remove_process_node(exitedNode);
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
    char *argument = tokens_get_token(tokens, 1);
    processID = is_integer(argument);
    if (processID == -1) {
      fprintf(stderr, "First argument must be an integer process ID\n");
      return -1;
    }
  } else {
    /* 
     * if there's no second argument, move the most recent subprocess to
     * the foreground
     */
    struct process_node *processNode = get_latest_process();
    if (processNode == NULL) {
      return 0;
    }
    processID = processNode->processID;
  }

  /*
   * first set the subprocess's process group as the foreground, then
   * send a resume signal to the process, in case it was suspended
   */
  pid_t processGroupID = get_process_group(processID);
  if (processGroupID == -1) {
    fprintf(stderr, "Failed to find process\n");
    return -1;
  }
  
  int result = set_foreground_process_group(SHELL_INPUT_FILE_NUM, processID);
  if (result == -1) {
    fprintf(stderr, "Failed to move process to foreground\n");
    return -1;
  }
  start_process(processID);

  /*
   * similar to the shell, wait until the foreground process to pause or exit,
   * then reclaim the foreground. If processes exited normally, also remove it
   * from the process list
   */
  result = wait_till_pause_or_exit(processID);
  if (result == -1) {
    return -1;
  } else if (WIFEXITED(result)) {
    remove_process(processID);
  }

  result = set_foreground_process_group(SHELL_INPUT_FILE_NUM, get_process_group(get_process()));
  if (result == -1) {
    return -1;
  }
  return 0;
}

/**
 * A built-in command of the shell. Resumes a paused background process.
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
    char *argument = tokens_get_token(tokens, 1);
    processID = is_integer(argument);
    if (processID == -1) {
      fprintf(stderr, "First argument must be an integer process ID\n");
      return -1;
    }
  } else {
    // if there's no second argument, use the most recent subprocess
    struct process_node *processNode = get_latest_process();
    if (processNode == NULL) {
      return 0;
    }
    processID = processNode->processID;
  }

  /*
   * since the shell process was in the foreground, the given subprocess was
   * definitely in the background. And since this command sets no new process
   * group to the foreground, the shell will continue to be the foreground,
   * and we only have to make sure that the given background process was resumed
   */
  return start_process(processID);
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