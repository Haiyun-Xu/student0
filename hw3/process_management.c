/**
 * This module contains helper functions for managing processes.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "process_management.h"

static struct process_node *PROCESS_LIST_HEAD = NULL;
static pid_t FOREGROUND_PROCESS_GROUP_ID = -1;

/**
 * Create a process node on heap memory.
 * 
 * @param processID The ID of the process
 * @param prev The pointer to the previous process node
 * @param next The pointer to the next process node
 * 
 * @return <struct process_node*> Pointer to the created process node, or NULL on error
 */
struct process_node *create_process_node(pid_t processID, struct process_node * prev, struct process_node * next) {
  struct process_node *processNode = (struct process_node *) malloc(sizeof(struct process_node));
  if (processNode == NULL) {
    perror("Failed to allocate memory: ");
    return NULL;
  }

  processNode->processID = processID;
  processNode->prev = prev;
  processNode->next = next;
  return processNode;
}

/**
 * Destroy the given process node.
 * 
 * @param processNode The pointer to the process node to be destroyed
 * 
 * @return void
 */
void destroy_process_node(struct process_node *processNode) {
  if (processNode == NULL) {
    return;
  }
  
  free(processNode);
  return;
}

/**
 * Initialize the process list managed by the shell.
 * 
 * @return int Returns 0 if successful, or -1 if not
 */
int initialize_process_list() {
  // edge cases
  if (PROCESS_LIST_HEAD != NULL) {
    return 0;
  }

  PROCESS_LIST_HEAD = create_process_node(-1, NULL, NULL);
  if (PROCESS_LIST_HEAD == NULL) {
    return -1;
  } else {
    return 0;
  }
}

/**
 * Desctroy the process list managed by the shell.
 * 
 * @return void
 */
void destroy_process_list() {
  // edge cases
  if (PROCESS_LIST_HEAD == NULL) {
    return;
  }

  struct process_node *current = PROCESS_LIST_HEAD, *next = NULL;
  PROCESS_LIST_HEAD = NULL;
  while (current != NULL) {
    next = current->next;
    destroy_process_node(current);
    current = next;
  }
  return;
}

/**
 * Return the latest process in the process list.
 * 
 * @return pid_t The process ID
 */
pid_t get_latest_process() {
  if (PROCESS_LIST_HEAD == NULL) {
    return -1;
  }

  return PROCESS_LIST_HEAD->processID;
}

/**
 * Find a process in the process list.
 * 
 * @return <struct process_node *> The pointer to the process node, or NULL
 *                                 if the process was not found
 */
struct process_node *find_process(pid_t processID) {
  // edge cases, do not return the root process node
  if (PROCESS_LIST_HEAD == NULL || processID == -1) {
    return NULL;
  }

  struct process_node *processNode = PROCESS_LIST_HEAD;
  while (processNode != NULL) {
    if (processNode->processID == processID) {
      return processNode;
    } else {
      processNode = processNode->next;
    }
  }
  return NULL;
}

/**
 * Add a process to the process list managed by the shell. If the process list
 * was not initialized, this function initializes it as well.
 * 
 * @param processID The ID of the process to be added
 * 
 * @return int Returns 0 if successful, or -1 if failed
 */
int add_process(pid_t processID) {
  // edge cases
  if (PROCESS_LIST_HEAD == NULL) {
    int result = initialize_process_list();
    if (result == -1) {
      fprintf(stderr, "Unable to add process to the process list\n");
      return -1;
    }
  }

  struct process_node *processNode = create_process_node(processID, NULL, PROCESS_LIST_HEAD);
  if (processNode == NULL) {
    fprintf(stderr, "Unabled to add process to the process list\n");
    return -1;
  }
  PROCESS_LIST_HEAD->prev = processNode;
  PROCESS_LIST_HEAD = processNode;
  return 0;
}

/**
 * Remove a process from the process list.
 * 
 * @param processID The ID of the process to be removed
 * 
 * @return void
 */
void remove_process(pid_t processID) {
  // edge cases
  if (PROCESS_LIST_HEAD == NULL) {
    fprintf(stderr, "Failed to remove process from process list\n");
    return;
  }

  struct process_node *processNode = find_process(processID);
  if (processNode == NULL) {
    fprintf(stderr, "Did not find processin process list\n");
    return;
  }

  
  /*
  * since find_process() does not return the root process node, we don't
  * need to consider the corner case where the returned process node is
  * the last one in the process list
  */
  if (processNode->prev == NULL) {
    /*
     * if the process node is the first one in the list, we need to update
     * its successor and the head of the process list
     */
    processNode->next->prev = NULL;
    PROCESS_LIST_HEAD = processNode->next;
  } else {
    // otherwise, update both its predecessor and its successor
    processNode->prev->next = processNode->next;
    processNode->next->prev = processNode->prev;
  }
  destroy_process_node(processNode);
  return;
}

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
 * Set the process group of the given process as the foreground process group
 * associated with the given controlling terminal.
 * 
 * @param terminalFileNum The file descriptor number of the terminal
 * @param processGroupID The process group ID
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int set_foreground_process(int terminalFileNum, pid_t processID) {
  // check that the given process ID is a valid subprocess of the shell
  struct process_node *processNode = find_process(processID);
  if (processNode == NULL) {
    fprintf(stderr, "Must provide a valid subprocess of the shell\n");
    return -1;
  }

  return set_foreground_process_group(terminalFileNum, get_process_group(processNode->processID));
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
  int result = kill(processID, SIGCONT);
  if (result == -1) {
    perror("Failed to send signal to process: ");
    return -1;
  }
  return 0;
}

/**
 * Wait untill ALL processes in the given list have either paused or exited.
 * 
 * NOTE: if any process does not process/exit predictably, then this
 * function will never return.
 * 
 * @param processIDs The list of process IDs
 * 
 * @return int Returns 0 if all process paused/exited, or -1 otherwise
 */
int wait_till_all_processes_pause_or_exit(pid_t *processIDs) {
  // edge cases
  if (processIDs == NULL) {
    return 0;
  }

  pid_t processID = -1;
  for (int index = 0; processIDs[index] != -1; index++) {
    // wait for each process until it is suspended or terminated
    processID = waitpid(processIDs[index], NULL, WUNTRACED);
    if (processID != processIDs[index]) {
      perror("Failed to wait for process to pause or exit: ");
      return -1;
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
 * @param terminalInputFileNum The input file descriptor number of the terminal
 * @param terminalOutputFileNum The output file descriptor number of the terminal
 * @param backgroundExecution Whether the process should be placed in the background
 * 
 * @return int Returns 0 if successful, or -1 otherwise;
 */
int manage_shell_subprocesses(pid_t *subprocessIDs, int terminalInputFileNum, int terminalOutputFileNum, bool backgroundExecution) {
  // edge cases
  if (subprocessIDs == NULL) {
    return -1;
  }

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
  int result = 0;
  result = wait_till_all_processes_pause_or_exit(subprocessIDs);
  if (result == -1) {
    return -1;
  }

  // for (int index = 0; subprocessIDs[index] != -1; index++) {
  //   if (add_process(subprocessIDs[index]) == -1) {
  //     fprintf(stderr, "Failed to manage subprocesses\n");
  //     return -1;
  //   }
  // }
  pid_t subprocessGroupID = group_processes(subprocessIDs);

  if (backgroundExecution) {
    // if the process should be executed in the background, simply let it resume
    result = start_process_group(subprocessGroupID);
    return result;
  } else {
    /*
     * otherwise, the shell should donate the foreground status to the
     * subprocess group, so that all terminal signals are sent directly to
     * the subprocess group
     */
    result = set_foreground_process_group(terminalInputFileNum, subprocessGroupID);
    if (result == -1) {
      return -1;
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

    result = wait_till_all_processes_pause_or_exit(subprocessIDs);
    if (result == -1) {
      return -1;
    }

    result = set_foreground_process_group(terminalInputFileNum, get_process_group(get_process()));
    if (result == -1) {
      return -1;
    }
  }

  return 0;
}