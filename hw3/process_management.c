/**
 * This module contains helper functions for managing processes.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "process_management.h"

static pid_t FOREGROUND_PROCESS_GROUP_ID = -1;

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
  // edge cases
  if (processID == -1) {
    return -1;
  }

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
  pid_t callingProcessGroupID = get_process_group(get_process());
  int result = 0;

  if (callingProcessGroupID == FOREGROUND_PROCESS_GROUP_ID) {
    /*
     * if the calling process is currently in the foreground, it can directly
     * donate the foreground status to the given process group
     */
    result = tcsetpgrp(terminalFileNum, processGroupID);
    if (result == -1) {
      perror("Failed to set process group as the terminal foreground: ");
      return -1;
    }
  } else {
    /*
     * otherwise, if the calling process is currently in the background, then
     * there's additional work involved. To reclaim the foreground status,
     * this process must use the tcsetpgrp() syscall; but since this is a
     * background process, tcsetpgrp() will send it a SIGTTOU, which will
     * interrupt the syscall and suspend this process.
     * 
     * In order to successfully reclaim the foreground, this process must
     * register a temporary handler that would ignore the SIGTTOUT signal and
     * restart the tcsetpgrp() syscall when interrupted.
     */
    struct sigaction newHandler, oldHandler;
    newHandler.sa_handler = SIG_IGN;
    newHandler.sa_flags = SA_RESTART;
    result = sigaction(SIGTTOU, &newHandler, &oldHandler);
    if (result == -1) {
      perror("Failed to register signal handler: ");
      return -1;
    }

    result = tcsetpgrp(terminalFileNum, processGroupID);
    if (result == -1) {
      perror("Failed to reclaim the terminal foreground: ");
      return -1;
    }

    // deregister the temporary SIGTTOU handler and reset it to the original handler
    result = sigaction(SIGTTOU, &oldHandler, NULL);
    if (result == -1) {
      // move on despite the old handler was not restored
      perror("Failed to deregister signal handler: ");
    }
  }

  FOREGROUND_PROCESS_GROUP_ID = processGroupID;
  return result;
}

/**
 * To start a suspended process group.
 * 
 * @param processGroupID The process group ID
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int start_process_group(pid_t processGroupID) {
  int result = kill(-processGroupID, SIGCONT);
  if (result == -1) {
    perror("Failed to send signal to process group: ");
    return -1;
  }
  return 0;
}

/**
 * To start a suspended process.
 * 
 * @param processID The process ID
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int start_process(pid_t processID) {
  // edge cases
  if (processID < 1) {
    return -1;
  }

  int result = kill(processID, SIGCONT);
  if (result == -1) {
    perror("Failed to send signal to process: ");
    return -1;
  }
  return 0;
}

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
int wait_till_pause_or_exit(pid_t processID) {
  // edge cases
  if (processID == -1) {
    return 0;
  }

  int processStatus = 0, result = 0;
  result = waitpid(processID, &processStatus, WUNTRACED);
  if (result != processID) {
    perror("Failed to wait for process to pause or exit: ");
    return -1;
  }
  return processStatus;
}

/**
 * Reap processes that are in the process list, but have exited.
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int reap_exited_processes() {
  struct process_node *processNode = get_latest_process(), *nextNode = NULL;

  pid_t processID = -1;
  int processStatus = 0;
  while (processNode != NULL) {
    nextNode = get_next_process(processNode);

    /*
     * wait for the subprocess's exit without blocking, and remove processes
     * that have exited
     */
    processID = waitpid(processNode->processID, &processStatus, WNOHANG);
    if (processID != 0 && WIFEXITED(processStatus)) {
      remove_process_node(processNode);
    }

    processNode = nextNode;
  }

  return 0;
}

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
int manage_shell_subprocesses(pid_t *subprocessIDs, bool backgroundExecution) {
  // edge cases
  if (subprocessIDs == NULL) {
    return -1;
  }

  /*
   * because the shell process has disabled passive/asynchronous zombie
   * reaping via SIG_CHLD, it must synchronously reap the subprocesses that
   * have exited and are in a zombie state. We prefer to do this before
   * running the new subprocesses, because the new subprocesses might need
   * the latest state on the previous subprocesses (such as `ps`)
   */
  reap_exited_processes();

  /*
   * Since the shell process has no idea whether a process has suspended
   * itself, if we naively send a SIGCONT to the process, it may result
   * in the signal arriving before the process suspends itself, and causing
   * the process to be indefinitely suspended.
   * 
   * Therefore, before calling this function, the shell process must first wait
   * for all the processes to be suspended, and only then can it send SIGCONT
   * to the process group.
   */
  for (int index = 0, subprocessStatus = 0; subprocessIDs[index] != -1; index++) {
    subprocessStatus = wait_till_pause_or_exit(subprocessIDs[index]);
    if (subprocessStatus == -1) {
      return -1;
    }
  }

  // add all subprocesses to the process list
  for (int index = 0; subprocessIDs[index] != -1; index++) {
    if (add_process(subprocessIDs[index]) == -1) {
      fprintf(stderr, "Failed to manage subprocesses\n");
      return -1;
    }
  }

  pid_t subprocessGroupID = group_processes(subprocessIDs);
  int result = 0;

  /*
   * if the program is to be executed in the foreground, the shell should
   * donate the foreground status to the subprocess group, so that all
   * terminal signals are sent directly to the subprocess group
   */
  if (!backgroundExecution) {
    result = set_foreground_process_group(SHELL_INPUT_FILE_NUM, subprocessGroupID);
    if (result == -1) {
      return -1;
    }
  }

  /*
    * after all the process management, allow the subprocesses to progress.
    * Since the subprocess group is in the foreground, the shell should wait
    * for all of the subprocesses to pause or exit before reclaiming the
    * foreground and returning
    */
  result = start_process_group(subprocessGroupID);
  if (result == -1) {
    return -1;
  }

  /*
   * if the program was executed in the foreground, the shell should wait
   * until it gets suspended or terminated, then reclaim the foreground
   * before returning
   */
  if (!backgroundExecution) {
    for (int index = 0, subprocessStatus = 0; subprocessIDs[index] != -1; index++) {
      subprocessStatus = wait_till_pause_or_exit(subprocessIDs[index]);
      // if processes exited normally, remove this process from the list
      if (subprocessStatus == -1) {
        return -1;
      } else if (WIFEXITED(subprocessStatus)) {
        remove_process(subprocessIDs[index]);
      }
    }

    result = set_foreground_process_group(SHELL_INPUT_FILE_NUM, get_process_group(get_process()));
    if (result == -1) {
      return -1;
    }
  }
  return 0;
}