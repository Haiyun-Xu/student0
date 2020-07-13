/**
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * 
 * An explanation of PINTOS list.h structure:
 * A list struct always contains a head and a tail. They are the sentinel
 * elements surrounding the internal elements.
 * 
 * In an empty list, we have [head (reverse beginning, element before tail)
 * => tail (beginning, element after head)], where front and back are
 * undefined. 
 * 
 * In a non-empty list, we have [head => front (beginning) => back (reverse
 * beginning) => tail].
 */

#include "custom_helpers.h"

/**
 * Given an array of file pointers, close all the files and free the array.
 * 
 * @param count The number of files to close
 * @param filePtrs The array of pointers of the files to be closed
 */
void closeFiles(int count, FILE** filePtrs) {
  // edge cases
  if (count <= 0 || filePtrs == NULL) return;

  FILE *filePtr = NULL;
  for (int filePtrIndex = 0; filePtrIndex < count; filePtrIndex++) {
    if ((filePtr = filePtrs[filePtrIndex]) == NULL) continue;
    fclose(filePtr);
  }
  free(filePtrs);
  return;
}

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
FILE** openFiles(int count, char* fileNamePtrs[]) {
  // edge cases
  if (fileNamePtrs == NULL || count <=0) return NULL;

  /*
   * zero-out the array, so that if we need to abort and close the files
   * prematurely, we dont treat garbage values as file pointers.
   */
  FILE **filePtrs = (FILE**) calloc(count, sizeof(FILE*));
  FILE* filePtr = NULL;
  char messageBuffer[100];

  for (int fileNameIndex = 0; fileNameIndex < count; fileNameIndex++) {
    char* fileName = fileNamePtrs[fileNameIndex];

    if((filePtr = fopen(fileName, "r")) == NULL) {
      closeFiles(count, filePtrs);
      sprintf(messageBuffer, "fopen() failed to open file %s.", fileName);
      perror(messageBuffer);
      return NULL;
    }

    filePtrs[fileNameIndex] = filePtr;
  }
  return filePtrs;
}

/**
 * A custom type for passing both the word count list and the input file
 * pointer as one argument to a new thread.
 */
typedef struct count_words_arg {
  word_count_list_t * wordCountListPtr;
  FILE *inputFilePtr;
} count_words_arg_t;

/**
 * Clean up the word count list.
 * 
 * @param wordCountListPtr The word count list to clean up
 */
void cleanUpWordCountList(word_count_list_t* wordCountListPtr) {
  /**
   * All threads using the word count list should exit before this function
   * is called, and the caller thread will be the only thread accessing the
   * word count list. This function therefore does not need to obtain the
   * mutex lock on the word_count_list. If threads were forecefully terminated,
   * they may have died holding the lock, in which case the lock wouldn't be
   * obtainable anyway.
   */

  // edge cases
  if (wordCountListPtr == NULL) return;

  struct list *lst = &(wordCountListPtr->lst);
  struct list_elem *lstElement = list_begin(lst);

  // if the list is empty, the execution skip this loop
  while (lstElement != list_tail(lst)) {
    /*
     * first update the pointer to the next list element, because the current
     * one will be freed at the end of the loop, and we won't be able to find
     * the next element then.
     */
    struct list_elem *currentElement = lstElement;
    lstElement = list_next(lstElement);

    // remove the current element from the list, then free its host word count
    currentElement = list_remove(currentElement);
    word_count_t *wordCount = list_entry(currentElement, word_count_t, elem);
    free(wordCount->word);
    free(wordCount);
  }

  free(wordCountListPtr);
  return;
}

/**
 * Wait for all the threads in the thread pool to exit before proceeding.
 * 
 * @param numOfThreads The number of threads in the thread pool
 * @param threadPool An array of pthread_t
 */
void joinThreadPool(int numOfThreads, pthread_t *threadPool) {
  // edge cases
  if (numOfThreads <= 0 || threadPool == NULL) return;

  for (int index = 0; index < numOfThreads; index++) {
    int exitError;
    int joiningError = pthread_join(threadPool[index], (void **)(&exitError));

    if (joiningError)
      printf("Failed to wait for thread %d to exit.", index);

    if (exitError)
      printf("Joined with thread %d which exited with status %d.", index, exitError);
  }
  return;
}


/**
 * Clean up the thread pool.
 * 
 * @param numOfThreads The number of threads in the thread pool
 * @param threadPool An array of pthread_t
 */
void cleanUpThreadPool(int numOfThreads, pthread_t *threadPool) {
  // edge cases
  if (numOfThreads <= 0 || threadPool == NULL) return;
  
  /*
  * send termination signal to the created threads, and wait for all of
  * them to exit before freeing the thread pool memory block, because the signals
  * are handled asynchronously by target threads.
  */
  for (int index = 0; index < numOfThreads; index++) {
    int signallingError = pthread_kill(threadPool[index], SIGTERM);

    if (signallingError)
      printf("Failed to send SIGTERM to thread %d.", index);
  }

  joinThreadPool(numOfThreads, threadPool);
  free(threadPool);
  return;
}

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
) {
  // edge cases
  if (numOfThreads <= 0) return;

  // terminate the threads first, so that other resources do not have users
  cleanUpThreadPool(numOfThreads, threadPool);
  cleanUpWordCountList(wordCountList);
  closeFiles(numOfThreads, filePtrs);
  free(argArray);
  return;
}

/**
 * A multi-threaded version of count_words(), accepting a struct instead
 * of two separate arguments as argument.
 * 
 * @param arg The struct containing the word_count_list_t* and the input File*
 */
void count_words_p(count_words_arg_t *arg) {
  return;
}