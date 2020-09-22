/**
 * This module contains helper functions for accessing the process list.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "process_list.h"

static struct process_node *PROCESS_LIST_HEAD = NULL;

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
 * Return the latest process node in the process list.
 * 
 * @return <struct process_node*> The process node
 */
struct process_node *get_latest_process() {
  if (PROCESS_LIST_HEAD == NULL || PROCESS_LIST_HEAD->processID == -1) {
    return NULL;
  }

  return PROCESS_LIST_HEAD;
}

/**
 * Returns the previous process node.
 * 
 * @param processNode A process node
 * 
 * @return <struct process_node*> The previous process node, or NULL if the
 *                                given one was the first one in the list
 */
struct process_node *get_prev_process(struct process_node *processNode) {
  if (processNode == NULL) {
    return NULL;
  }

  return processNode->prev;
}

/**
 * Returns the next process node.
 * 
 * @param processNode A process node
 * 
 * @return <struct process_node*> The next process node, or NULL if the given
 *                                one was the last one in the list
 */
struct process_node *get_next_process(struct process_node *processNode) {
  if (processNode == NULL) {
    return NULL;
  }

  // DO NOT return the root process node
  if (processNode->next != NULL && processNode->next->processID != -1) {
    return processNode->next;
  } else {
    return NULL;
  }
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
 * Remove a process node from the process list.
 * 
 * @param processNode The process node to be removed
 * 
 * @return void
 */
void remove_process_node(struct process_node *processNode) {
  // edge cases, DO NOT remove the root process node of the list
  if (processNode == NULL || processNode->processID == -1) {
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
  remove_process_node(processNode);
  return;
}