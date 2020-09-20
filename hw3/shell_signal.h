/**
 * This module contains helper functions for handling signals sent to the
 * shell process and its subprocesses.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef SHELL_SIGNAL_H
#define SHELL_SIGNAL_H

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "shell_config.h"
#include "process_management.h"

/**
 * Resets the current process's ignored signal handlers to the default hanlder.
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int reset_ignored_signals();

/**
 * Register signal handlers for the shell.
 * 
 * @return int Returns 0 if successful, or -1 if failed
 */
int register_shell_signal_handlers();

#endif /* SHELL_SIGNAL_H */