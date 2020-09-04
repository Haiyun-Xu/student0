/**
 * This module contains helper functions for general use.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef HELPERS_H
#define HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tokenizer.h"

/**
 * Duplicate the given string.
 * The caller should own the heap memory and free it.
 * 
 * @param string The string to duplicate
 * 
 * @return char* A duplicate of the given string
 */
char *duplicate_string(const char *string);

/**
 * Concatenate the two strings together, without modifying the originals.
 * The caller should own the heap memory and free it.
 * 
 * @param prefix The string to be in the front
 * @param suffix The string to be in the back
 * 
 * @return char* The concatenated string
 */
char *concatenate_strings(const char *prefix, const char *suffix);

/**
 * Join the strings together in the given order, separated by the separator.
 * The caller should own the heap memory and free it.
 * 
 * @param strings An array of strings, terminated by a NULL pointer
 * @param separator The character used to separate the strings
 * 
 * @return char* The joined string
 */
char *join_strings(char *const *strings, const char separator);

/**
 * Checks whether the tokens list is empty;
 * 
 * @param tokens The list of command tokens
 * 
 * @return Returns 1 if the tokens list is empty, or 0 otherwise
 */
int is_tokens_empty(struct tokens *tokens);

/**
 * Checks whether the file is executable.
 * 
 * @param filePath The full path of the file
 * 
 * @return int Returns 0 if the file is executable, or 1 if not;
 */
int is_file_executable(const char* filePath);

/**
 * Returns the full path of the given executable file.
 * 
 * The caller should own the heap memory and free it.
 * 
 * @param fileName Name of the executable file
 * 
 * @return char* The full path of the executable file, or NULL if the full
 *               path is not found, i.e. the executable file does not exist
 */
char *resolve_executable_full_path(const char *fileName);

#endif /* HELPERS_H */