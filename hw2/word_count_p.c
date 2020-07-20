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
 * be created in memory.
 * 
 * @param wclist The word count list, shared by multithreads
 */
void init_words(word_count_list_t *wclist) {
  pthread_mutex_lock(&(wclist->lock));

  // edge cases return immediately
  if (wclist != NULL)
    list_init(&(wclist->lst));

  pthread_mutex_unlock(&(wclist->lock));
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
 * @param wclist The word count list, shared by multithreads
 * @param word The target word
 * 
 * @return word_count_t* Pointer to the word_count_t containing the target
 * word; NULL if the word is not found in the list
 */
word_count_t *find_word(word_count_list_t *wclist, char *word) {
  pthread_mutex_lock(&(wclist->lock));

  word_count_t *wc = NULL;

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

  pthread_mutex_unlock(&(wclist->lock));
  return wc;
}

/**
 * Increment the count of a word.
 * 
 * @param wclist The word count list, shared by multithreads and containing
 *        the given word_count_t; its mutex lock is used to guarantee Exclusive
 *        Usage Rights of this function
 * @param wc The word_count_t containing the target word
 */
word_count_t *increment_count(word_count_list_t *wclist, word_count_t *wc) {
  pthread_mutex_lock(&(wclist->lock));

  // edge cases return immediately
  if (wclist != NULL && wc != NULL)
    wc->count += 1;

  pthread_mutex_unlock(&(wclist->lock));
  return wc;
}

/**
 * Create a word_count_t to contain the given word, then add the new node
 * to the given word count list. Must copy the content of word into a new
 * memory block.
 * 
 * @param wclist The word count list, shared by mutithreads; its mutex lock
 *        is used to guarantee Exclusive Usage Rights of this function
 * @param word The word to be contained in the new word_count_t
 */
word_count_t *create_word(word_count_list_t *wclist, char *word) {
  pthread_mutex_lock(&(wclist->lock));

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

  pthread_mutex_unlock(&(wclist->lock));
  return wc;
}

/**
 * Add a word to the word count list. If the word is not already present,
 * create the word node with count=1; if already present, increment its count.
 * 
 * @param wclist The word count list, shared by multithreads
 * @param word the target word
 * 
 * @return word_count_t* Pointer to the word_count_t containing the target
 * word; NULL if the arguments were wrong and the word cannot be added
 */
word_count_t *add_word(word_count_list_t *wclist, char *word) {
  // edge cases
  if (wclist == NULL || word == NULL) return NULL;

  word_count_t* wc = find_word(wclist, word);
  if (wc != NULL)
    wc = increment_count(wclist, wc);
  else
    wc = create_word(wclist, word);
  
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
