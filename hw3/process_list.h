/**
 * This module contains helper functions for accessing the process list.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef PROCESS_LIST_H
#define PROCESS_LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

struct process_node {
  pid_t processID;
  struct process_node *prev;
  struct process_node *next;
};

/**
 * Initialize the process list managed by the shell.
 * 
 * @return int Returns 0 if successful, or -1 if not
 */
int initialize_process_list();

/**
 * Desctroy the process list managed by the shell.
 * 
 * @return void
 */
void destroy_process_list();

/**
 * Return the latest process node in the process list.
 * 
 * @return <struct process_node*> The process node
 */
struct process_node *get_latest_process();

/**
 * Returns the previous process node.
 * 
 * @param processNode A process node
 * 
 * @return <struct process_node*> The previous process node, or NULL if the
 *                                given one was the first one in the list
 */
struct process_node *get_prev_process(struct process_node *processNode);

/**
 * Returns the next process node.
 * 
 * @param processNode A process node
 * 
 * @return <struct process_node*> The next process node, or NULL if the given
 *                                one was the last one in the list
 */
struct process_node *get_next_process(struct process_node *processNode);

/**
 * Find a process in the process list.
 * 
 * @return <struct process_node *> The pointer to the process node, or NULL
 *                                 if the process was not found
 */
struct process_node *find_process(pid_t processID);

/**
 * Add a process to the process list managed by the shell. If the process list
 * was not initialized, this function initializes it as well.
 * 
 * @param processID The ID of the process to be added
 * 
 * @return int Returns 0 if successful, or -1 if failed
 */
int add_process(pid_t processID);

/**
 * Remove a process node from the process list.
 * 
 * @param processNode The process node to be removed
 * 
 * @return void
 */
void remove_process_node(struct process_node *processNode);

/**
 * Remove a process from the process list.
 * 
 * @param processID The ID of the process to be removed
 * 
 * @return void
 */
void remove_process(pid_t processID);

#endif /* PROCESS_LIST_H */