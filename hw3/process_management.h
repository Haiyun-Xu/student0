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
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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
 * Reclaim the foreground status for the current process group.
 * 
 * @param terminalFileNum The file descriptor number of the terminal
 * 
 * @return int Returns 0 if successful, or 1 otherwise
 */
int reclaim_foreground_process_group(int terminalFileNum);

/**
 * To start a suspended process group.
 * 
 * Since the shell process has no idea whether a process has suspended
 * itself, if we naively send a SIGCONT to the process, it may result
 * in the signal arriving before the process suspends itself, and causing
 * the process to be indefinitely suspended.
 * 
 * Therefore, before calling this function, the shell process must first wait
 * for all the processes to be suspended, and only then can it send SIGCONT
 * to the process group.
 * 
 * @param processGroupID The process group ID
 * 
 * @return void
 */
void start_process_group(pid_t processGroupID);

/**
 * Wait untill ALL processes in the given list have suspended.
 * 
 * NOTE: if any process does not suspend predictably, then this function will
 * never return.
 * 
 * @param processIDs The list of process IDs
 * 
 * @return int Returns 0 if all process have been suspended, or 1 if any
 *             process was not suspended
 */
int wait_for_processes_to_pause(pid_t *processIDs);

/**
 * Wait until ALL processes in the given list have exited.
 * 
 * @param processIDs The list of process IDs
 * 
 * @return int Returns 0 if all processes exited successfully, or 1 if any
 *             process exited with an error status
 */
int wait_for_processes_to_exit(pid_t *processIDs);

/**
 * Manage a group of shell subprocesses by doing the following:
 * 1) wait for all the subprocesses to suspend themselves;
 * 2) place the subprocesses into the same process group;
 * 3) set that process group as the foreground of the terminal;
 * 4) alow the process group to execute new program images;
 * 5) wait for the process group to exit;
 * 6) reclaim the terminal foreground for the shell;
 * 
 * @param processIDs The list of subprocessIDs
 * @param terminalInputFileNum The input file descriptor number of the terminal
 * @param terminalOutputFileNum The output file descriptor number of the terminal
 * 
 * @return int Returns 0 if successful, or -1 otherwise;
 */
int manage_shell_subprocesses(pid_t *subprocessIDs, int terminalInputFileNum, int terminalOutputFileNum);

#endif /* PROCESS_MANAGEMENT_H */