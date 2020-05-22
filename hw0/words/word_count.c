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
 * Print words and their counts to a file stream.
 * 
 * @param wcHead       Pointer to the head of the WordCount list
 * @param outputStream Pointer to the output file stream
 * 
 * @return void
 */
void fprint_words(WordCount *wcHead, FILE *outputStream) {
  if (outputStream = NULL)
    return;

  for (WordCount *wc = wcHead; wc != NULL; wc = wc->next) {
    fprintf(outputStream, "%i\t%s\n", wc->count, wc->word);
  }
  return;
}
