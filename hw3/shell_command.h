/**
 * This module contains built-in commands for the shell.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef SHELL_COMMAND_H
#define SHELL_COMMAND_H

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "process_management.h"
#include "shell_config.h"
#include "tokenizer.h"

// convenience macro to silence compiler warnings about unused function parameters.
#define unused __attribute__((unused))

// built-in command functions take tokens (list of command arguments) and return int
typedef int command_t(struct tokens *tokens);

/**
 * A built-in command of the shell. Brings up the help menu and prints a helpful description for the given command
 * 
 * @return int Returns 0 if successful, pr -1 otherwise
 */
int cmd_help(unused struct tokens *tokens);

/**
 * A built-in command of the shell. Exits this shell process.
 * 
 * @return void
 */
int cmd_exit(unused struct tokens *tokens);

/**
 * A built-in command of the shell. Prints the current working directory in the file system.
 * 
 * @return int Returns 0 if successful, pr -1 otherwise
 */
int cmd_pwd(unused struct tokens *tokens);

/**
 * A built-in command of the shell. Changes the current working directory in the file system.
 * 
 * @param tokens The list of command arguments
 * 
 * @return int Returns 0 if successful, pr -1 otherwise
 */
int cmd_cd(struct tokens *tokens);

/**
 * A built-in command of the shell. Moves a given process to the foreground.
 * 
 * @param tokens The list of command arguments
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int cmd_fg(struct tokens *tokens);

/**
 * A built-in command of the shell. Moves a given process to the background.
 * 
 * @param tokens The list of command arguments
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int cmd_bg(struct tokens *tokens);

/**
 * Look up and return the built-in command function.
 * 
 * @param command The command string
 * 
 * @return command_t* The command function, or NULL otherwise
 */
command_t *shell_command_lookup(char* command);

#endif /* SHELL_COMMAND_H */