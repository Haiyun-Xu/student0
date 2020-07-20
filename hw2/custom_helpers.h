/**
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 */

#ifndef CUSTOM_HELPERS_H
#define CUSTOM_HELPERS_H

#include <pthread.h>
#include <signal.h>
#include "word_count.h"

/**
 * A custom type for passing both the word count list and the input file
 * pointer as one argument to a new thread.
 */
typedef struct count_words_arg {
  word_count_list_t * wordCountListPtr;
  FILE *inputFilePtr;
} count_words_arg_t;

/**
 * Given an array of file pointers, close all the files and free the array.
 * 
 * @param count The number of files to close
 * @param filePtrs The array of pointers of the files to be closed
 */
void closeFiles(int count, FILE** filePtrs);

/**
 * Given an array of filenames, open all the files and return their pointers
 * in an array.
 * 
 * @param count The number of files to open
 * @param fileNamePtrs The array of names of the files to be opened
 * 
 * @return FILE** The array of opened file pointers; NULL if any file failed
 * to open.
 */
FILE** openFiles(int count, char* fileNamePtrs[]);


/**
 * Clean up the word count list.
 * 
 * @param wordCountListPtr The word count list to clean up
 */
void cleanUpWordCountList(word_count_list_t* wordCountListPtr);

/**
 * Wait for all the threads in the thread pool to exit before proceeding.
 * 
 * @param numOfThreads The number of threads in the thread pool
 * @param threadPool An array of pthread_t
 */
void joinThreadPool(int numOfThreads, pthread_t *threadPool);

/**
 * Clean up the thread pool.
 * 
 * @param numOfThreads The number of threads in the thread pool
 * @param threadPool An array of pthread_t
 */
void cleanUpThreadPool(int numOfThreads, pthread_t *threadPool);

/**
 * Abort the word count and clean up all the dynamically allocated data.
 * 
 * @param numOfThreads The #threads that the caller tried to create
 * @param threadPool An array of pthread_t
 * @param wordCountList A word count list
 * @param filePtrs An array of FILE*
 * @param argArray An array of count_words_arg_t
 */
void abortWordCount(
  int numOfThreads,
  pthread_t *threadPool,
  word_count_list_t *wordCountList,
  FILE **filePtrs,
  count_words_arg_t *argArray
);

/**
 * A multi-threaded version of count_words(), accepting a struct instead
 * of two separate arguments as argument.
 * 
 * @param arg The struct containing the word_count_list_t* and the input File*
 * 
 * @return void* The return value indicates whether the function succeeded (0)
 *         or failed (non-zero values); it is only casted as void* to conform
 *         to the pthread_create() interface.
 */
void *count_words_p(void *arg);

/**
 * Returns whether the first word is less than the second word. First use
 * the count for ordering, then use the alphabetical order if the count was
 * a tie.
 * 
 * @param wc1 The first word_count_t
 * @param wc2 The second worc_count_t
 * 
 * @return bool Whether the first word is less than the second word
 */
bool less_word_p(const word_count_t *wc1, const word_count_t *wc2);

#endif