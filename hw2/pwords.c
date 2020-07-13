/*
 * Word count application with one thread per input file.
 *
 * You may modify this file in any way you like, and are expected to modify it.
 * Your solution must read each input file from a separate thread. We encourage
 * you to make as few changes as necessary.
 */ 

/*
 * Copyright © 2019 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "word_count.h"
#include "word_helpers.h"
#include "custom_helpers.h"

/*
 * main - handle command line, spawning one thread per file.
 */
int main(int argc, char *argv[]) {
  /* Create the empty data structure. */
  word_count_list_t wordCountList;
  init_words(&wordCountList);

  if (argc <= 1) {
    /* Process stdin in a single thread. */
    count_words(&wordCountList, stdin);
  } else {
    /**
     * open each of the files, create child threads, then pass them the routine
     * count_words_p() and the argument struct to each of the child threads.
     */
    int numOfThreads = argc - 1, threadCreationResult;
    FILE **filePtrs = openFiles(numOfThreads, &(argv[1]));
    count_words_arg_t *arguments = malloc(sizeof(count_words_arg_t) * numOfThreads);
    pthread_t *threadPool = malloc(sizeof(pthread_t) * numOfThreads);

    for (int index = 0; index < numOfThreads; index++) {
      // populate the argument struct
      count_words_arg_t argument = arguments[index];
      argument.wordCountListPtr = &index;
      argument.inputFilePtr = filePtrs[index];

      int creationError = pthread_create(&(threadPool[index]), NULL, count_words_p, (void *)(&argument));
      if (creationError) {
        printf("Failed to create thread %d", index);
        abortWordCount(numOfThreads, threadPool, &wordCountList, filePtrs, arguments);
        return 1;
      }
    }

    // wait for all the child threads to complete before proceeding
    joinThreadPool(numOfThreads, threadPool);
  }

  /** TODO: implement count_words_p() and the multithreaded methods that it uses */

  /* Output final result of all threads' work. */
  wordcount_sort(&wordCountList, less_count);
  fprint_words(&wordCountList, stdout);
  return 0;
}
