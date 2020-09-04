/**
 * This module contains helper functions for managing processes.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "process_management.h"

/**
 * Place the given list of processes into the same process group, which is
 * numbered after and headed by the first process in the list.
 * 
 * @param processIDs The list of processes
 * 
 * @return pid_t The process group ID, or -1 on failure
 */
pid_t group_processes(pid_t *processIDs) {
  // edge cases
  if (processIDs == NULL || processIDs[0] == -1) {
    return -1;
  }

  pid_t processGroupID = processIDs[0];
  int result = 0;
  for (int index = 0; processIDs[index] != -1; index++) {
    result = setpgid(processIDs[index], processGroupID);
    /*
     * even if one subprocess fails to be assigned, we should still try to
     * assign the other subprocesses
     */
    if (result == -1) {
      perror("Failed to assign subprocess to process group: ");
    }
  }
  return processGroupID;
}

/**
 * Returns the process ID of the calling process.
 * 
 * @return pid_t The calling process's ID
 */
pid_t get_process() {
  return getpid();
}

/**
 * Returns the process group ID of the given process.
 * 
 * @param processID A process ID;
 * 
 * @return pid_t The process group ID of the given process; returns -1 if failed
 */
pid_t get_process_group(pid_t processID) {
  pid_t processGroupID = getpgid(processID);
  if (processGroupID == -1) {
    perror("Failed to get process group ID: ");
  }
  return processGroupID;
}

/**
 * Set the given process group as the foreground process group associated
 * with the given controlling terminal.
 * 
 * @param terminalFileNum The file descriptor number of the terminal
 * @param processGroupID The process group ID
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int set_foreground_process_group(int terminalFileNum, pid_t processGroupID) {
  int result = tcsetpgrp(terminalFileNum, processGroupID);
  if (result == -1) {
    perror("Failed to set process group as the terminal foreground: ");
  }
  return result;
}

/**
 * Reclaim the foreground status for the current process group.
 * 
 * @param terminalFileNum The file descriptor number of the terminal
 * 
 * @return int Returns 0 if successful, or 1 otherwise
 */
int reclaim_foreground_process_group(int terminalFileNum) {
  int overallResult = 0, result = 0;

  /*
   * To reclaim the foreground status, this process must use the tcsetpgrp()
   * syscall; but since this process is in the background, tcsetpgrp() will 
   * send this process a SIGTTOU and interrupt the syscall.
   * 
   * In order to successfully reclaim the foreground, this process must
   * register a temporary handler that would ignore the SIGTTOUT signal and
   * restart the tcsetpgrp() syscall when interrupted.
   */
  struct sigaction newHandler, oldHandler;
  newHandler.sa_handler = SIG_IGN;
  newHandler.sa_flags = SA_RESTART;
  result = sigaction(SIGTTOU, &newHandler, &oldHandler);
  overallResult = overallResult || result;
  if (result == -1) {
    // move on despite the handler was not registered
    perror("Failed to register signal handler: ");
  }

  result = set_foreground_process_group(terminalFileNum, get_process_group(get_process()));
  overallResult = overallResult || result;
  if (result == -1) {
    // move on despite the process didn't reclaim the foreground
    perror("Failed to reclaim the terminal foreground: ");
  }

  // deregister the temporary SIGTTOU handler and reset it to the original handler
  result = sigaction(SIGTTOU, &oldHandler, NULL);
  overallResult = overallResult || result;
  if (result == -1) {
    // move on despite the old handler was not restored
    perror("Failed to deregister signal handler: ");
  }

  return overallResult;
}

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
void start_process_group(pid_t processGroupID) {
  int result = kill(-processGroupID, SIGCONT);
  if (result == -1) {
    perror("Failed to send signal to process group: ");
  }
  return;
}

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
int wait_for_processes_to_pause(pid_t *processIDs) {
  // edge cases
  if (processIDs == NULL) {
    return 0;
  }

  int processStatus = 0;
  for (int index = 0; processIDs[index] != -1; index++) {
    // wait for each process until it is suspended
    while (true) {
      waitpid(processIDs[index], &processStatus, WUNTRACED);
      if (WIFSTOPPED(processStatus)) {
        break;
      }
    }
  }
  return 0;
}

/**
 * Wait until ALL processes in the given list have exited/paused.
 * 
 * @param processIDs The list of process IDs
 * 
 * @return int Returns 0 if all processes exited successfully, or 1 if any
 *             process exited with an error status
 */
int wait_for_processes_to_exit(pid_t *processIDs) {
  // edge cases
  if (processIDs == NULL) {
    return 0;
  }

  int processID = -1, processStatus = 0, overallStatus = 0;
  for (int index = 0; processIDs[index] != -1; index++) {
    /*
     * wait until the process exits or pauses. If either stat change occurs,
     * there is no reason keep that process in the foreground, as it will
     * only keep the terminal hanging. So when either state change occurs,
     * the foreground should be reclaimed.
     */
    processID = waitpid(processIDs[index], &processStatus, WUNTRACED);
    
    /*
     * if any process in the group exited with an error status, consider
     * the entire process group exited with error staus
     */
    overallStatus = overallStatus || processStatus;
    if (processID == -1 && !WIFEXITED(processStatus)) {
      perror("Failed to wait for child process to terminate: ");
    }
  }
  return overallStatus;
}

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
int manage_shell_subprocesses(pid_t *subprocessIDs, int terminalInputFileNum, int terminalOutputFileNum) {
  // edge cases
  if (subprocessIDs == NULL) {
    return -1;
  }

  /*
   * wait for all the subprocesses to suspend themselves, so that they can
   * be placed in the same process group before calling execve()
   */
  wait_for_processes_to_pause(subprocessIDs);
  pid_t subprocessGroupID = group_processes(subprocessIDs);

  /*
   * the shell donates the foreground status to the subprocess group,
   * so that all terminal signals are sent directly to the subprocess group
   */
  set_foreground_process_group(terminalInputFileNum, subprocessGroupID);

  /*
   * after all the process management, allow the subprocesses to progress,
   * and wait for them to exit before resuming to the shell process and
   * returning to it the goreground status
   */
  start_process_group(subprocessGroupID);
  wait_for_processes_to_exit(subprocessIDs);
  reclaim_foreground_process_group(terminalInputFileNum);
  return 0;
}