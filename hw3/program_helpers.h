/**
 * This module contains helper functions for non built-in programs.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef PROGRAM_HELPERS_H
#define PROGRAM_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tokenizer.h"

/**
 * Duplicate the given string.
 * Caller should own the heap memory and free it.
 * 
 * @param string The string to duplicate
 * 
 * @return char* A duplicate of the given string
 */
char *duplicate_string(const char *string);

/**
 * Concatenate the two strings together, without modifying the originals.
 * Caller should own the heap memory and free it.
 * 
 * @param prefix The string to be in the front
 * @param suffix The string to be in the back
 * 
 * @return char* The concatenated string
 */
char *concatenate_strings(const char *prefix, const char *suffix);

/**
 * Checks whether the file is executable.
 * 
 * @param filePath The full path of the file
 * 
 * @return int Returns 0 if the file is executable, or 1 if not;
 */
int is_file_executable(const char* filePath);

/**
 * Return the program's full path on the heap.
 * Caller should own the heap memory and free it.
 * 
 * @param programName The name of the program
 * 
 * @return char* Pointer to the full path of the program, or NULL if it can't be found
 */
char *get_program_full_path(char *programName);

/**
 * Get the program name from the command.
 * The returned string is not newly-allocated.
 * 
 * @param programName The address of a pointer to string
 * @param tokens The tokens struct containing all the command parts
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_program_name(char **programName, struct tokens *tokens);

/**
 * Get the program arguments from the command.
 * The program arguments are not newly-allocated, but the pointer array to the program arguments is.
 * Caller should own the heap memory and free it.
 * 
 * @param argList The address of a pointer to a list of strings
 * @param tokens The tokens struct containing all the command parts
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_program_arguments(char ***argList, struct tokens *tokens);

/**
 * Execute non built-in programs as a new process.
 * 
 * @param tokens The tokens struct containing all the command parts
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int exec_program(struct tokens *tokens);

#endif /* PROGRAM_HELPERS_H */