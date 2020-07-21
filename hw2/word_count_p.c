/*
 * Implementation of the word_count interface using Pintos lists and pthreads.
 *
 * You may modify this file, and are expected to modify it.
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

/**
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
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

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_lp.c"
#endif

#ifndef PTHREADS
#error "PTHREADS must be #define'd when compiling word_count_lp.c"
#endif

#include "word_count.h"

/**
 * Initialize a word count list (shared). The word count list should already
 * be created in memory, and has an uninitialized pthread_mutex_t lock.
 * 
 * @param wclist The word count list, shared by multithreads
 */
void init_words(word_count_list_t *wclist) {
  /*
   * this function should be called by the main thread, when no child thread
   * has been created yet. Since the mutex lock is not yet initialized, it
   * cannot be acquired.
   * 
   * the lock has been instantiated but not initialized, so it is in the
   * locked state (owned by pid=0, the system) and cannot be acquired.
   * In fact, pthread_mutex_trylock() returned EINVAL=22 when called
   * immediately on wclist->lock, without initialization.
   */
  pthread_mutex_init(&(wclist->lock), NULL);

  // edge cases return immediately
  if (wclist != NULL)
    list_init(&(wclist->lst));

  return;
}

/**
 * Get length of a word count list (shared).
 * 
 * @param wclist Pointer to the word count list, shared by multithreads
 * 
 * @return size_t Length of the word count list
 */
size_t len_words(word_count_list_t *wclist) {
  pthread_mutex_lock(&(wclist->lock));

  size_t length = 0;
  
  // edge cases return immediately
  if (wclist != NULL) { 
    struct list *lst = &(wclist->lst);
    struct list_elem *element = list_begin(lst);

    while (element != list_end(lst)) {
      length++;
      element = list_next(element);
    }
  };

  pthread_mutex_unlock(&(wclist->lock));
  return length;
}

/**
 * Find a word in a word_count list (shared). Return the word_count_t if
 * found, or return NULL if not found.
 * 
 * WARN: this function should only be called by a function that holds the
 * mutex lock.
 * 
 * @param wclist The word count list, shared by multithreads
 * @param word The target word
 * 
 * @return word_count_t* Pointer to the word_count_t containing the target
 * word; NULL if the word is not found in the list
 */
word_count_t *find_word(word_count_list_t *wclist, char *word) {
  word_count_t *wc = NULL;

  // edge cases return immediately
  if (wclist != NULL && word != NULL) {
    struct list *lst = &(wclist->lst);
    struct list_elem *element = list_begin(lst);

    while (element != list_end(lst)) {
      word_count_t *tempWc = list_entry(element, word_count_t, elem);
      if (!strcmp(word, tempWc->word)) {
        wc = tempWc;
        break;
      }
      element = list_next(element);
    }
  }

  return wc;
}

/**
 * Increment the count of a word.
 * 
 * WARN: this function should only be called by a function that holds the
 * mutex lock.
 * 
 * @param wclist The word count list, shared by multithreads and containing
 *        the given word_count_t; its mutex lock is used to guarantee Exclusive
 *        Usage Rights of this function
 * @param wc The word_count_t containing the target word
 */
word_count_t *increment_count(word_count_list_t *wclist, word_count_t *wc) {
  // edge cases return immediately
  if (wclist != NULL && wc != NULL)
    wc->count += 1;

  return wc;
}

/**
 * Create a word_count_t to contain the given word, then add the new node
 * to the given word count list. Must copy the content of word into a new
 * memory block.
 * 
 * WARN: this function should only be called by a function that holds the
 * mutex lock.
 * 
 * @param wclist The word count list, shared by mutithreads; its mutex lock
 *        is used to guarantee Exclusive Usage Rights of this function
 * @param word The word to be contained in the new word_count_t
 */
word_count_t *create_word(word_count_list_t *wclist, char *word) {
  word_count_t *wc = NULL;

  // edge cases return immediately
  if (wclist != NULL && word != NULL) {
    wc = (word_count_t *) malloc(sizeof(word_count_t));
    char *wcWord = (char *) malloc(sizeof(char) * (strlen(word) + 1));
    
    // memory allocation failure causes immediate return
    if (wc != NULL && wcWord != NULL) {
      strcpy(wcWord, word);
      wc->word = wcWord;
      wc->count = 1;
      list_push_front(&(wclist->lst), &(wc->elem));
    }
  }

  return wc;
}

/**
 * Add a word to the word count list (shared). If the word is not already present,
 * create the word node with count=1; if already present, increment its count.
 * Always free the given word.
 * 
 * WARN: the find and increment/create logic MUST be executed thoroughly
 * within a critical section, with no breaks in between. The reason is that,
 * if thread A released the lock after not being able to find the word_count_t
 * containing the target word, thread B could acquire the lock and be looking
 * for the same word. Both of them would fail to find the word and both insert
 * a new word_count_t into the list. However (depending on the search algorithm),
 * with a find() function that implements linear search, only the latter
 * inserted word_count_t, i.e. the one in the front, will be incremented
 * when that word appears again, because the former inserted word_count_t
 * will never be returned from the find() function. 
 * 
 * @param wclist The word count list, shared by multithreads
 * @param word the target word
 * 
 * @return word_count_t* Pointer to the word_count_t containing the target
 * word; NULL if the arguments were wrong or if the word cannot be added
 */
word_count_t *add_word(word_count_list_t *wclist, char *word) {
  pthread_mutex_lock(&(wclist->lock));
  
  word_count_t* wc = NULL;
  /*
   * edge cases; the address of wclist is not an internal state and is
   * shared by all threads. Accessing it does not require locking.
   */
  if (wclist == NULL || word == NULL) return NULL;

  /*
   * the address of a word_count_t within the list is not an internal state
   * of the word count list either, and it is constant throughout the lifetime
   * of the word count list. Accessing it does not require locking.
   */
  wc = find_word(wclist, word);
  if (wc != NULL)
    wc = increment_count(wclist, wc);
  else
    wc = create_word(wclist, word);
  
  pthread_mutex_unlock(&(wclist->lock));
  return wc;
}

/**
 * Print word counts to a file from a list (shared).
 * 
 * @param wclist The word count list, shared by multithreads
 * @param outfile The output file stream
 */
void fprint_words(word_count_list_t *wclist, FILE *outfile) {
  pthread_mutex_lock(&(wclist->lock));
  
  // edge case exits immediately
  if (outfile != NULL) {
    struct list *lst = &(wclist->lst);
    struct list_elem *element = list_begin(lst);

    while (element != list_end(lst)) {
      word_count_t *wc = list_entry(element, word_count_t, elem);
      fprintf(outfile, "%8d\t%s\n", wc->count, wc->word);

      element = list_next(element);
    }
  }

  pthread_mutex_unlock(&(wclist->lock));
  return;
}

/**
 * A comparator returning true if the first list element is less than
 * the second list element, or false if the first is equal to or larger
 * than the second element.
 * 
 * This function must be called by a parent function with the Exclusive
 * Usage Right over the shared list to which the given list elements belong.
 * 
 * @param first The first list element
 * @param second The second list element
 * @param less_comparator Pointer to an auxilary comparator function
 * 
 * @return bool Whether the first list element is less than the second list
 *         element
 */
bool list_elem_less_comparator(
  const struct list_elem *first,
  const struct list_elem *second,
  void *less_comparator
) {
  // edge cases
  if (first == NULL || second == NULL || less_comparator == NULL) return false;

  word_count_t *wcFirst = list_entry(first, word_count_t, elem);
  word_count_t *wcSecond = list_entry(second, word_count_t, elem);
  bool (*comparator) (const word_count_t*, const word_count_t*) = less_comparator;
  return comparator(wcFirst, wcSecond);
}

/**
 * Sort a word count list using the provided comparator function.
 * 
 * @param wclist The word count list, shared by multithreads
 * @param less A comparator function
 */
void wordcount_sort(
  word_count_list_t *wclist,
  bool less(const word_count_t *, const word_count_t *)
) {
  pthread_mutex_lock(&(wclist->lock));

  // edge cases return immediately
  if (wclist != NULL)
    list_sort(&(wclist->lst), list_elem_less_comparator, less);

  pthread_mutex_unlock(&(wclist->lock));
  return;
}
