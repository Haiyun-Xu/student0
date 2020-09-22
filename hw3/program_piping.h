/**
 * This module contains helper functions for piped programs.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef PROGRAM_PIPING_H
#define PROGRAM_PIPING_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tokenizer.h"
#include "helpers.h"
#include "shell_signal.h"

/**
 * Checks whether the command line contains input/output piping.
 * Piping is always of the form "[prgram A] | [program B]".
 * 
 * @param tokens The list of command tokens
 * 
 * @return int Returns 1 if there's input/output piping, or 0 if not
 */
int contains_piping(struct tokens *tokens);

/**
 * Parse the command tokens into an array of program names and an array of
 * argument lists. The returned program names and arguments are not
 * newly-allocated, but the program name array, argument array, and argument
 * lists are.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param tokens The list of command tokens
 * @param programNamesPtr The address of the program name array
 * @param argListsPtr The address of the argument lists array
 * @param bgExecPtr The address of the background execution flag
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int parse_piping_tokens(struct tokens *tokens, char ***programNamesPtr, char ****argListsPtr, bool *bgExecPtr);

/**
 * Execute the given programs with the given arguments, accounting for the
 * piping options. The subprocesses will be suspended until they receive a
 * SIG_CONT.
 * 
 * Takes the ownership of argument list and frees it.
 * 
 * @param programNames The array of program names
 * @param programArgLists The array of program argument lists; will be freed by this function
 * 
 * @return pid_t* Returns the list of subprocess IDs
 */
pid_t *execute_piped_program(const char **programNames, const char ***programArgLists);

#endif /* PROGRAM_PIPING_H */