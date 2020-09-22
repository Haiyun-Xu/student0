/**
 * This module contains the configurations of the shell.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef SHELL_CONFIG_H
#define SHELL_CONFIG_H

#include <stdbool.h>
#include <sys/types.h>
#include <termios.h>

#define COMMAND_LINE_LENGTH 4096

// Whether the shell is connected to an actual terminal or not
bool shellIsInteractive;

// File descriptor to use as the shell's input and output file
int SHELL_INPUT_FILE_NUM;
int SHELL_OUTPUT_FILE_NUM;

// Terminal mode settings for the shell
struct termios shell_tmodes;

// the shell's process group ID, and the terminal's foreground process group ID
pid_t shellProcessGroupID;
pid_t terminalForegroundProcessGroupID;

#endif /* SHELL_CONFIG_H */