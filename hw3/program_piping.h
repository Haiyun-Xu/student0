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

/**
 * Checks whether the given argument is the pipe symbol.
 * 
 * @param argument An argument string
 * 
 * @return int Returns 1 if the argument is a pipe symbol, or 0 if not
 */
int is_pipe_symbol(const char *argument);

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
 * Count the number of processes within the pipeline.
 * 
 * @param tokens The list of command tokens
 * 
 * @return int The number of processes in the pipeline. If tokens is:
 *             NULL => 0;
 *             [] => 0;
 *             [A] => 1;
 *             [A | ] => 1;
 *             [A | B] => 2;
 *             [A | B | ] => 2;
 *             ...
 */
int count_piped_processes(struct tokens *tokens);

/**
 * Free the list of file paths.
 * 
 * @param filePaths A list of file paths, end with a NULL pointer;
 *                  Both the list and the file path strings are dynamically-allocated
 * 
 * @return void
 */
void free_file_paths(char **filePaths);

/**
 * Free the list of program names.
 * 
 * @param programNames A list of program names, end with a NULL pointer;
 *                     Only the list was dynamically-allocated, the program
 *                     name strings are not
 * 
 * @return void
 */
void free_program_names(char **programNames);

/**
 * Free the array of argument lists.
 * 
 * @param programArgLists An array of lists, end wtih a NULL pointer;
 *                        Only the array and its argument lists are dynamically-
 *                        allocated, the arguments are not
 * 
 * @return void
 */
void free_program_argument_lists(char ***programArgLists);

/**
 * Returns the full paths of the given executable files. Both the list and
 * each of the full paths are newly-allocated.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param fileNames Names of the executable file
 * 
 * @return char** The list of full paths of the executable files, or NULL
 *                if the full path of any one file is not found, i.e. the
 *                executable file does not exist
 */
char** resolve_executable_full_paths(const char **fileNames);

/**
 * Return the program names from the command tokens in a NULL terminated
 * array of strings. The strings are not newly-allocated, but the string
 * array is.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param tokens The list of command tokens
 * @param programNamesPtr The address of a list of program names
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed 
 */
int get_piped_program_names(struct tokens *tokens, char ***programNamesPtr);

/**
 * Return the program arguments from the command tokens in a NULL terminated
 * array of argument lists. The returned arguments are not newly-allocated,
 * but the argument list and the argument list array are.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param tokens The list of command tokens
 * @param programArgListsPtr The address of an array of argument lists
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_piped_program_arguments(struct tokens *tokens, char ****programArgListsPtr);

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
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int parse_piping_tokens(struct tokens *tokens, char ***programNamesPtr, char ****argListsPtr);

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