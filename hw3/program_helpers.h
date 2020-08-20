/**
 * This module contains helper functions for non built-in programs.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef PROGRAM_HELPERS_H
#define PROGRAM_HELPERS_H

#include <fcntl.h>
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
 * Join the strings together in the given order, separated by the separator.
 * Caller should own the heap memory and free it.
 * 
 * @param strings An array of strings, terminated by a NULL pointer
 * @param separator The character used to separate the strings
 * 
 * @return char* The joined string
 */
char *join_strings(char *const *strings, const char separator);

/**
 * Checks whether the file is executable.
 * 
 * @param filePath The full path of the file
 * 
 * @return int Returns 0 if the file is executable, or 1 if not;
 */
int is_file_executable(const char* filePath);

/**
 * Return the program's name.
 * 
 * @param tokens The list of command arguments
 * 
 * @return char* The program name in string
 */
char *get_program_name(struct tokens *tokens);

/**
 * Return the program's full path on the heap.
 * Caller should own the heap memory and free it.
 * 
 * @param tokens The list of command arguments
 * @param programName The name of the program
 * 
 * @return int Returns 0 if successful, or -1 if failed
 */
int get_program_full_path(struct tokens *tokens, char **programFullPath);

/**
 * Get the program arguments from the command.
 * The program arguments are not newly-allocated, but the pointer array to the program arguments is.
 * Caller should own the heap memory and free it.
 * 
 * @param tokens The tokens struct containing all the command parts
 * @param argList The address of a pointer to a list of strings
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int get_program_arguments(struct tokens *tokens, char ***argList);

/**
 * Interpret command arguments of program call, and resolve the program full
 * path and the list of program parameters.
 * Caller should own the heap memory and free it.
 * 
 * @param tokens The list of command arguments
 * @param programFullPath Address of a string, where the program full path will be stored
 * @param programArgList Address of an array of strings, where the program call arguments will be stored
 * 
 * @return int Returns 0 if the parsing succeeded, or -1 if failed
 */
int interpret_command_arguments(struct tokens *tokens, char **programFullPath, char ***programArgList);

/**
 * Execute the given program with the given arguments.
 * 
 * @param programFullPath The full path to the program executable
 * @param programArgList The list of program arguments
 * 
 * @return int Returns 0 if the program executed successfully, or -1 otherwise
 */
int execute_program(const char *programFullPath, char *const *programArgList);

#endif /* PROGRAM_HELPERS_H */