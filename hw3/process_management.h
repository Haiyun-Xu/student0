/**
 * This module contains helper functions for managing processes.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef PROCESS_MANAGEMENT_H
#define PROCESS_MANAGEMENT_H

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shell_config.h"
#include "process_list.h"

/**
 * Place the given list of processes into the same process group, which is
 * numbered after and headed by the first process in the list.
 * 
 * @param processIDs The list of processes
 * 
 * @return pid_t The process group ID, or -1 on failure
 */
pid_t group_processes(pid_t *processIDs);

/**
 * Returns the process ID of the calling process.
 * 
 * @return pid_t The calling process's ID
 */
pid_t get_process();

/**
 * Returns the process group ID of the given process.
 * 
 * @param processID A process ID;
 * 
 * @return pid_t The process group ID of the given process; returns -1 if failed
 */
pid_t get_process_group(pid_t processID);

/**
 * Set the given process group as the foreground process group associated
 * with the given controlling terminal.
 * 
 * @param terminalFileNum The file descriptor number of the terminal
 * @param processGroupID The process group ID
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int set_foreground_process_group(int terminalFileNum, pid_t processGroupID);

/**
 * To start a suspended process group.
 * 
 * @param processGroupID The process group ID
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int start_process_group(pid_t processGroupID);

/**
 * To start a suspended process.
 * 
 * @param processID The process ID
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int start_process(pid_t processID);

/**
 * Wait untill a process has either paused or exited.
 * 
 * NOTE: if the process does not pause/exit predictably, then this
 * function will hang.
 * 
 * @param processID The process ID to wait for
 * 
 * @return int Returns the process exit status if the process paused/exited
 *             successfully, or -1 otherwise
 */
int wait_till_pause_or_exit(pid_t processID);

/**
 * Manage a group of shell subprocesses by doing the following:
 * 1) wait for all of the subprocesses to suspend themselves;
 * 2) add all subprocesses to the shell subprocess list;
 * 3) place the subprocesses into the same process group;
 * 4) depending on the background-execution flag, either:
 *    a) set that process group in the background, allow it to resume,
 *    and return, or;
 *    a) set that process group as the foreground of the terminal, allow it
 *    to resume, then wait for all of the subprocesses to exit, and reclaim
 *    the terminal foreground for the shell;
 * 
 * @param processIDs The list of subprocessIDs
 * @param backgroundExecution Whether the process should be placed in the background
 * 
 * @return int Returns 0 if successful, or -1 otherwise;
 */
int manage_shell_subprocesses(pid_t *subprocessIDs, bool backgroundExecution);

#endif /* PROCESS_MANAGEMENT_H */