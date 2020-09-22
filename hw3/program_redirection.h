/**
 * This module contains helper functions for I/O redirected programs.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef PROGRAM_REDIRECTION_H
#define PROGRAM_REDIRECTION_H

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "helpers.h"
#include "tokenizer.h"
#include "shell_signal.h"

/**
 * Checks whether the command line contains input/output redirection.
 * Redirection is always of the form "[prgram] > [file]" or "[prgram] < [file]".
 * 
 * @param tokens The list of command tokens
 * 
 * @return int Returns -1/1 if there's input/output redirection, or 0 if no redirection
 */
int contains_redirection(struct tokens *tokens);

/**
 * Parse the command tokens into a program name, an argument list, and a
 * redirection file name. The returned program name, arguments, and file
 * name are not newly-allocated, but the argument list is.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param tokens The command tokens
 * @param programNamePtr The address of the program name
 * @param argListPtr The address of the argument list
 * @param fileNamePtr The address of the redirection file name
 * @param bgExecPtr The address of the background execution flag
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int parse_redirection_tokens(struct tokens *tokens, char **programNamePtr, char ***argListPtr, char **fileNamePtr, bool *bgExecPtr);

/**
 * Execute the given program with the given arguments, accounting for the
 * redirection options. The subprocess will be suspended until it receives
 * a SIG_CONT.
 * 
 * Takes the ownership of argument list and frees it.
 * 
 * @param programName The program name
 * @param programArgList The list of program arguments; will be freed by this function
 * @param redirectionType The redirection type: -1 for input redirect, 1 for
 *                        output redirect, and 0 for no redirect
 * @param redurectionFileName The redirection file name
 * 
 * @return pid_t* Returns a list of subprocess IDs
 */
pid_t *execute_redirected_program(const char *programName, char **programArgList, int redirectionType, const char *redirectionFileName);

#endif /* PROGRAM_REDIRECTION_H */