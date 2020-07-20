/**
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * 
 * NOTE: An explanation of PINTOS list.h structure:
 * A list struct always contains a head and a tail. They are the sentinel
 * elements surrounding the internal elements.
 * 
 * In an empty list, we have [head (reverse beginning, element before tail)
 * => tail (beginning, element after head)], where front and back are
 * undefined. 
 * 
 * In a non-empty list, we have [head => front (beginning) => back (reverse
 * beginning) => tail].
 * 
 * NOTE: All of the following functions must acquire the lock before any
 * action, and release the lock before returning. Even the edge case check
 * needs to occur after acquiring exclusive usage right (EUR), because if
 * the check passes and only then does the function acquires the lock, it
 * may proceed with the wrong assumption that the shared object is still
 * valid; in our case, the shared object pointed by the argument pointer
 * could well be freed by another thread (when the current thread is
 * interrupted and waiting) and hence not-existent anymore.
 * 
 * This means that we are accessing a pointer without checking if it's valid
 * first, and certainlycan trigger segfault if the argument pointer is NULL.
 * But that's better than mandating that the pointer must be checked again
 * after acquisition of the lock - which is confusing and easy to forget.
 * 
 * NOTE: The functions that directly operate on the shared object, i.e. 
 * acquires/releases the shared object's lock, must not exit early, i.e. 
 * return between the lock acquire/release lines.
 * 
 * That will leave the shared object's ownership in a "claimed" state
 * and cause a deadlock. To prevent a return value discrepancy, we can
 * declare the default return value right after acquiring the lock, and then
 * check for edge cases and allow edge cases to skip the main logic and 
 * immediately return the default return value.
 * 
 * NOTE: Functions that do not directly operate on the shared object DO NOT
 * need to acquire/release the lock on the shared object. This is especially
 * important when a function uses other functions that directly operate on
 * the shared object: if the parent function claimed the lock and becomes
 * blocked before the child function returns, the child function wouldn't be
 * able to acquire the lock and will cause a deadlock.
 * 
 *  pthread_mutex_lock(&(wclist->lock));
 *  pthread_mutex_unlock(&(wclist->lock));
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
 * in an array. If an error occurred, return a NULL pointer.
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
  if (filePtrs == NULL) return NULL;

  FILE* filePtr = NULL;
  char messageBuffer[100];

  for (int fileNameIndex = 0; fileNameIndex < count; fileNameIndex++) {
    char* fileName = fileNamePtrs[fileNameIndex];

    if((filePtr = fopen(fileName, "r")) == NULL) {
      sprintf(messageBuffer, "fopen() failed to open file %s.\n", fileName);
      perror(messageBuffer);

      closeFiles(count, filePtrs);
      return NULL;
    }

    filePtrs[fileNameIndex] = filePtr;
  }
  return filePtrs;
}

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
  while (lstElement != list_end(lst)) {
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
    /*
     * exitError will contain the exit value of the thread starter routine;
     * in this case it will be an interger
     */
    int exitError;
    int joiningError = pthread_join(threadPool[index], (void **)(&exitError));

    if (joiningError)
      printf("Failed to wait for thread %d to exit.\n", index);

    if (exitError)
      printf("Joined with thread %d which exited with status %d.\n", index, exitError);
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
  
  // send termination signal to the created threads
  for (int index = 0; index < numOfThreads; index++) {
    int signallingError = pthread_kill(threadPool[index], SIGTERM);

    if (signallingError)
      printf("Failed to send SIGTERM to thread %d.\n", index);
  }

  /* 
   * wait for all the threads to exit before freeing the thread pool memory
   * block, because the signals are handled asynchronously by target threads.
   */
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
 * Extract the next word from the file.
 * 
 * @param file The input file pointer
 * 
 * @return char* pointer to the extracted word
 */
static char *get_word_p(FILE* file) {
  // edge cases
  if (file == NULL) return NULL;

  // first skip over all non-alphabetical characters at the beginning;
  // then extract the characters one at a time, converting them to lower
  // case, and adding them to the word;
  // loop until a non-alphabetical character is found;
  // append a null-char to the end of the word, and return the address of the word

  /*
   * skip over all non-alphabetical characters, until an EOF or an alphabet
   * is met. In the first case, the file index has reached the end, so return
   * NULL; in the second case, break out of the loop and proceed
   */
  int character;
  while (!isalpha(character = fgetc(file))) {
    if (character == EOF)
      return NULL;
  }

  int index = 0, word_size = 16;
  char *word = (char*) malloc(word_size * sizeof(char));
  if (word == NULL) {
    printf("Failed to allocate memory for new word in file.\n");
    return NULL;
  }

  do {
    /*
     * if the index is at the last byte , we must double the size of the word
     * buffer by reallocating
     */
    if (index == (word_size - 1)) {
      word_size *= 2;
      char *new_word = (char*) realloc(word, word_size);
      if (new_word == NULL) {
        printf("Failed to re-allocate memory for new word in file.\n");
        free(word);
        return NULL;
      }
      word = new_word;
    }

    word[index++] = tolower(character);
  } while (isalpha(character = fgetc(file)));

  // remember to terminate the word buffer with a nul-char
  word[index] = '\0';
  return word;
}

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
void *count_words_p(void *arg) {
  count_words_arg_t *argument = (count_words_arg_t *) arg;
  word_count_list_t *wclist = argument->wordCountListPtr;
  FILE *inputFile = argument->inputFilePtr;

  char* word;

  while ((word = get_word_p(inputFile)) != NULL) {
    // only count words that are longer than one alphabet
    if (strlen(word) == 1)
      free(word);
    // if the function failed to add the word to the list
    else if (add_word(wclist, word) == NULL) {
      free(word);
      return (void*) 1;
    }
  }
  return (void*) 0;
}

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
bool less_word_p(const word_count_t *wc1, const word_count_t *wc2) {
  // edge cases
  if (wc1 == NULL || wc2 == NULL) return false;

  if (wc1->count < wc2->count) return true;
  else if (wc1->count > wc2->count) return false;
  else return (strcmp(wc1->word, wc2->word) < 0);
}
