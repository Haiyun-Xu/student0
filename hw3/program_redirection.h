/**
 * This module contains helper functions for I/O redirected programs.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef PROGRAM_REDIRECTION_H
#define PROGRAM_REDIRECTION_H

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tokenizer.h"
#include "program_helpers.h"


/**
 * Checks whether the given argument is the redirection symbol.
 * 
 * @param argument An argument string
 * 
 * @return int Returns -1 if the argument is an input redirection symbol,
 *             returns 1 if it's an output redirection symbol, and 0 if
 *             not a redirection symbol
 */
int is_redirection_symbol(const char* argument);

/**
 * Checks whether the command line contains input/output redirection.
 * Redirection is always of the form "[prgram] > [file]" or "[prgram] < [file]".
 * 
 * @param tokens The list of command arguments
 * 
 * @return int Returns -1/1 if there's input/output redirection, or 0 if no redirection
 */
int contains_redirection(struct tokens *tokens);

/**
 * Get the redirected program call from the command.
 * The returned token struct is newly-allocated, and the
 * caller should own the heap memory and free it.
 * 
 * @param tokens The list of command arguments
 * @param programCallTokens Address of a pointer to the list of program call arguments
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_redirection_program_call(struct tokens *tokens, struct tokens **programCallTokens);

/**
 * Get the redirect file name from the command.
 * The returned string is not newly-allocated.
 * 
 * @param tokens The list of command arguments
 * @param fileName Address of a pointer to the filename
 * 
 * @return int Returns -1/1 if there's input/output redirection, or 0 if no redirection
 */
int get_redirection_file_name(struct tokens *tokens, char **fileName);

/**
 * Execute the given program with the given arguments, accounting for the redirection options.
 * 
 * @param programFullPath The full path to the program executable
 * @param programArgList The list of program arguments
 * @param redirectionType The redirection type: -1 for input redirect, 1 for
 *                        output redirect, and 0 for no redirect
 * @param redurectionFileName The redirection file name
 * 
 * @return int Returns 0 if the program executed successfully, or -1 otherwise
 */
int execute_program_redirected(const char *programFullPath, char *const *programArgList, int redirectionType, const char *redirectionFileName);

#endif /* PROGRAM_REDIRECTION_H */