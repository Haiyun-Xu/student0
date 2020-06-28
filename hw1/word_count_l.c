/*
 * Implementation of the word_count interface using Pintos lists.
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

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_l.c"
#endif

#include "word_count.h"

/**
 * Initialize a word count list.
 * 
 * @param wclist Pointer to a struct list
 * 
 * @return void
 */
void init_words(word_count_list_t *wclist) {
  // edge case
  if (wclist == NULL) return;

  return list_init(wclist);
}

/**
 * Get length of a word count list.
 * 
 * @param wclist Pointer to a struct list
 * 
 * @return Length of the struct list
 */
size_t len_words(word_count_list_t *wclist) {
  // edge case
  if (wclist == NULL) return 0;

  return list_size(wclist);
}

/**
 * Find a word in a word_count list.
 * 
 * @param wclist Pointer to a struct list
 * @param word Pointer to a string
 * 
 * @return Pointer to the word_count_t that contains the target word,
 *         or a NULL pointer if the word was not found in the struct list
 */
word_count_t *find_word(word_count_list_t *wclist, char *word) {
  // edge case
  if (wclist == NULL || word == NULL) return NULL;

  /*
   * Since only interior elements have a word_count_t host (neither head
   * nor tail do), we can't use list_entry() on either head or tail. So we
   * must start from the beginning of the list, and iterate till we've reached
   * the tail.
   */
  struct list_elem *listElement = list_begin(wclist);
  struct list_elem *tail = list_tail(wclist);
  while (listElement != tail) {
    // switch to the word_count_t host struct
    word_count_t * wcNode = list_entry(listElement, word_count_t, elem);
    if (strcmp(word, wcNode->word) == 0) return wcNode;
    else listElement = list_next(listElement);
  }
  return NULL;
}

/**
 * Insert word with count, if not already present; increment count if present.
 * Takes ownership of word.
 * 
 * @param wclist Pointer to a struct list
 * @param word Pointer to a string
 * @param count An integer associated with the word
 * 
 * @return Pointer to the word_count_t that contains the target word
 */
word_count_t *add_word_with_count(
  word_count_list_t *wclist,
  char *word,
  int count
) {
  // edge case
  if (wclist == NULL || word == NULL) return NULL;

  /*
   * If the word already exists, increment the count; if not, insert a new
   * word_count_t into the given list.
   */
  word_count_t *wcNode = find_word(wclist, word);
  if (wcNode != NULL)
    wcNode->count += 1;
  else {
    // instantiate a new word_count_t
    wcNode = (word_count_t *) malloc(sizeof(word_count_t));
    wcNode->word = (char *) malloc(sizeof(char) * (strlen(word) + 1));
    strcpy(wcNode->word, word);
    wcNode->count = count;
    // insert the new word_count_t into the list
    list_push_front(wclist, &(wcNode->elem));
  }
  return wcNode;
}

/**
 * Insert word with count=1, if not already present; increment count if
 * present. Takes ownership of word.
 * 
 * @param wclist Pointer to a struct list
 * @param word Pointer to a string
 * 
 * @return Pointer to the word_count_t that contains the target word
 */
word_count_t *add_word(word_count_list_t *wclist, char *word) {
  return add_word_with_count(wclist, word, 1);
}

/**
 * Print word counts to a file.
 * 
 * @param wclist Pointer to a struct list
 * @param outfile Pointer to the output FILE
 * 
 * @return void
 */
void fprint_words(word_count_list_t *wclist, FILE *outfile) {
  // edge case
  if (wclist == NULL || outfile == NULL) return;

  /*
   * Since only interior elements have a word_count_t host (neither head
   * nor tail do), we can't use list_entry() on either head or tail. So we
   * must start from the beginning of the list, and iterate till we've reached
   * the tail.
   */
  struct list_elem *listElement = list_begin(wclist);
  struct list_elem *tail = list_tail(wclist);
  while (listElement != tail) {
    word_count_t *wcNode = list_entry(listElement, word_count_t, elem);
    fprintf(outfile, "%8d\t%s\n", wcNode->count, wcNode->word);
    listElement = list_next(listElement);
  }
  return;
}

/**
 * Compares the host of two list_elem by count; if the counts are the same,
 * compare using the alphabetical order of the words in the hosts.
 * 
 * @param listElement1 Pointer to the first list_elem
 * @param listElement2 Pointer to the second list_elem
 * 
 * @return Whether the host of the first list_elem comes before that of
 *         the second
 */
static bool less_list(
  const struct list_elem *listElement1,
  const struct list_elem *listElement2,
  void *aux
) {
  // edge case
  if (listElement1 == NULL || listElement2 == NULL) return false;

  /*
   * The two list_elem can only be compared if they have a word_count_t host,
   * i.e. if they are both interior elements. Interior elements doe not have
   * NULL prev/next pointer.
   */
  if (
    listElement1->prev == NULL
    || listElement1->next == NULL
    || listElement2->prev == NULL
    || listElement2->next == NULL
  )
    return false;

  word_count_t * wcNode1 = list_entry(listElement1, word_count_t, elem);
  word_count_t * wcNode2 = list_entry(listElement2, word_count_t, elem);

  /*
   * First compare by the count in the host; if there's a tie, compare by
   * alphabetical order
   */
  if (wcNode1->count < wcNode2->count) return true;
  else if (wcNode1->count > wcNode2->count) return false;
  else return strcmp(wcNode1->word, wcNode2->word) < 0;
}

/**
 * Sort a word count list using the provided comparator function.
 * 
 * @param wclist Pointer to a struct list
 * @param less Function that compares two word_count_t
 * 
 * @return void
 */
void wordcount_sort(
  word_count_list_t *wclist,
  bool less(const word_count_t *, const word_count_t *)
) {
  list_sort(wclist, less_list, less);
}
