/**
 * This module contains helper functions for general use.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef HELPERS_H
#define HELPERS_H

#include <ctype.h>
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
 * Clean the entire string by overwriting it with null char.
 * 
 * @param string The string to be cleaned
 * @param length The length of the string
 */
void clean_string(char *string, int length);

/**
 * Checks whether the tokens list is empty;
 * 
 * @param tokens The list of command tokens
 * 
 * @return Returns 1 if the tokens list is empty, or 0 otherwise
 */
int is_tokens_empty(struct tokens *tokens);

/**
 * Checks whether the string is a non-negative integer.
 * 
 * @param string The string
 * 
 * @return int Returns the integer if the conversion is possible, or return -1
 */
int is_integer(const char *string);

/**
 * Checks whether the program should be executed in the background. A program
 * with the background execution flag always has the form "[command] &".
 * 
 * @param tokens The list of command tokens
 * 
 * @return int Returns 1 if there's the program should be executed in the
 *             background, or 0 otherwise
 */
int should_execute_in_background(struct tokens *tokens);

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