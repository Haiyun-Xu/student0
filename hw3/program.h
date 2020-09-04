/**
 * This module contains helper functions for non built-in programs.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef PROGRAM_H
#define PROGRAM_H

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "helpers.h"
#include "tokenizer.h"

/**
 * Return the program name.
 * 
 * @param tokens The list of command tokens
 * 
 * @return char* The program name
 */
char *get_program_name(struct tokens *tokens);

/**
 * Get the program arguments from the command.
 * The program arguments are not newly-allocated, but the pointer array to the program arguments is.
 * Caller should own the heap memory and free it.
 * 
 * @param tokens The list of command arguments
 * @param argListPtr The address of an argument list
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_program_arguments(struct tokens *tokens, char ***argListPtr);

/**
 * Parse the command tokens into a program name and an argument list. The
 * returned program name and arguments are not newly-allocated, but the
 * argument list is.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param tokens The command tokens
 * @param programNamePtr The address of the program name
 * @param argListPtr The address of the argument list
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int parse_tokens(struct tokens *tokens, char **programNamePtr, char ***argListPtr);

/**
 * Execute the given program with the given arguments. The subprocess will
 * be suspended until it receives a SIG_CONT.
 * 
 * @param programName The program name
 * @param programArgList The list of program arguments; will be freed by this function
 * 
 * @return pid_t* Returns a list of subprocess IDs
 */
pid_t *execute_program(const char *programName, char **programArgList);

#endif /* PROGRAM_H */