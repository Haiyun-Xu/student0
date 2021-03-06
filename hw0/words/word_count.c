/*

Copyright © 2019 University of California, Berkeley

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

word_count provides lists of words and associated count

Functional methods take the head of a list as first arg.
Mutators take a reference to a list as first arg.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "word_count.h"

/* Basic utililties */

/**
 * Replicate the given string, and return a copy in heap memory.
 * 
 * @param str Pointer to a c-string
 * 
 * @return Pointer to the replicated string
 */ 
char *new_string(char *str) {
  return strcpy((char *)malloc(strlen(str)+1), str);
}

/**
 * Initialize a word count list, updating the reference to the list.
 * 
 * @param wcList Pointer to a list of WordCount structs
 * 
 * @return void
 */ 
void init_words(WordCount **wcList) {
  *wcList = NULL;
  return;
}

/**
 * Clear the WordCount list by freeing all the heap memory allocated to it.
 * 
 * @param wcList Pointer to the WordCount list
 * 
 * @return void
 */
void clear_list(WordCount **wcList) {
  if (wcList == NULL) {
    return;
  }

  WordCount *wc = (*wcList), *wcNext = NULL;
  (*wcList) = NULL; // clear the pointer to the first WordCount

  while (wc != NULL) {
    wcNext = wc->next;
    free(wc->word);
    free(wc);
    wc = wcNext;
  }
  return;
}

/**
 * Length of a word count list.
 * 
 * @param wcHead Pointer to the head of the WordCount list
 * 
 * @return size_t Size of the WordCount list
 */
size_t len_words(WordCount *wcHead) {
    size_t len = 0;

    while (wcHead != NULL) {
      len++;
      wcHead = wcHead->next;
    }

    return len;
}

/**
 * Find a word in a word_count list.
 * 
 * @param wcHead Pointer to the head of the WordCount list
 * @param word   Pointer to the target word
 * 
 * @return WordCount|NULL The WordCount struct that contains the target word;
 *                        if the target word does not exist, return NULL
 */ 
WordCount *find_word(WordCount *wcHead, char *word) {
  if (word == NULL)
    return NULL;
  
  // strcmp() returns 0 if two strings are equal
  while (wcHead != NULL) {
    if (!strcmp(wcHead->word, word))
      return wcHead;
    wcHead = wcHead->next;
  }
  return NULL;
}

/**
 * Insert word with count=1, if not already present; increment count if present.
 * 
 * @param wcList Pointer to a list of WordCount structs
 * @param word   Pointer to the word to be added into the list
 * 
 * @return void
 */
void add_word(WordCount **wcList, char *word) {
  if (word == NULL)
    return;
  
  WordCount *wc = find_word((*wcList), word);
  if (wc != NULL) {
    // increment count if the word is present
    wc->count += 1;
  } else {
    // initialize a new WordCount struct to contain the new word
    wc = (WordCount*) malloc(sizeof(WordCount));
    wc->word = new_string(word);
    wc->count = 1;

    // prepend the new WordCount at the beginning of the list
    wc->next = (*wcList);
    *wcList = wc;
  }
}

/**
 * Combine the source and extension list, one word at a time. If a word in
 * the extension list exists in the source list, its count will be added to
 * the count in the source list; if a word in the extension list is absent
 * from the source list, it will be added to the source list.
 * 
 * NOTE: The order of words in the returned source list is not guaranteed.
 * The extension list will be untouched, so if the source list was empty,
 * it will contain replicates of all the words in the extension list.
 * 
 * @param wcListSource    Pointer to the source list of WordCounts
 * @param wcListExtension Pointer to the extension list
 * 
 * @return void
 */
void combine_lists(WordCount **wcListSource, WordCount **wcListExtension) {
  if ((*wcListExtension) == NULL) {
    return;
  }

  WordCount *wcToCombine = (*wcListExtension);
  while (wcToCombine != NULL) {
    WordCount *wcToJoin = find_word((*wcListSource), wcToCombine->word);

    if (wcToJoin != NULL) {
      wcToJoin->count += wcToCombine->count;
    } else {
      wcToJoin = (WordCount*) malloc(sizeof(WordCount));
      wcToJoin->word = new_string(wcToCombine->word);
      wcToJoin->count = wcToCombine->count;
      wcToJoin->next = (*wcListSource);
      *wcListSource = wcToJoin;
    }
    wcToCombine = wcToCombine->next;
  }
  return;
}

/**
 * Print words and their counts to a file stream.
 * 
 * @param wcHead       Pointer to the head of the WordCount list
 * @param outputStream Pointer to the output file stream
 * 
 * @return void
 */
void fprint_words(WordCount *wcHead, FILE *outputStream) {
  if (outputStream == NULL)
    return;

  for (WordCount *wc = wcHead; wc != NULL; wc = wc->next) {
    fprintf(outputStream, "%i\t%s\n", wc->count, wc->word);
  }
  return;
}

/**
 * Comparator to sort list by frequency, use alphabetical order as a secondary order.
 * 
 * @param wc1 The first WordCount operand
 * @param wc2 The second WordCount operand
 * 
 * @return bool Whether the first WordCount comes before the second WordCount
 */
bool wordcount_less(const WordCount *wc1, const WordCount *wc2) {
  // if either pointer is NULL, it is the lesser one
  if (wc1 == NULL) {
    return true;
  } else if (wc2 == NULL) {
    return false;
  }

  // if either has a lower count, it is the lesser one
  if (wc1->count < wc2->count) {
    return true;
  } else if (wc1->count > wc2->count) {
    return false;
  } else {
    // if there's a tie, the one with the lower alphabetical order is the lesser one
    return (strcmp(wc1->word, wc2->word) < 0);
  }
}
