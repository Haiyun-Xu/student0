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

*/

/*
word_count provides lists of words and associated count

Functional methods take the head of a list as first arg.
Mutators take a reference to a list as first arg.
*/



#ifndef word_count_h
#define word_count_h

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Representation of a word count object.
   Includes next field for constructing singly linked list*/
struct word_count {
    char *word;
    int count;
    struct word_count *next;
};

/* Introduce a type name for the struct */
typedef struct word_count WordCount;

/**
 * Replicate the given string, and return a copy in heap memory.
 * 
 * @param str Pointer to a c-string
 * 
 * @return Pointer to the replicated string
 */ 
char *new_string(char *str);

/**
 * Initialize a word count list, updating the reference to the list.
 * 
 * @param wcList Pointer to a list of WordCount structs
 * 
 * @return void
 */ 
void init_words(WordCount **wclist);

/**
 * Clear the WordCount list by freeing all the heap memory allocated to it.
 * 
 * @param wcList Pointer to the WordCount list
 * 
 * @return void
 */
void clear_list(WordCount **wcList);

/**
 * Length of a word count list.
 * 
 * @param wcHead Pointer to the head of the WordCount list
 * 
 * @return size_t Size of the WordCount list
 */
size_t len_words(WordCount *wchead);

/**
 * Find a word in a word_count list.
 * 
 * @param wcHead Pointer to the head of the WordCount list
 * @param word   Pointer to the target word
 * 
 * @return WordCount|NULL The WordCount struct that contains the target word;
 *                        if the target word does not exist, return NULL
 */ 
WordCount *find_word(WordCount *wchead, char *word);

/**
 * Insert word with count=1, if not already present; increment count if present.
 * 
 * @param wcList Pointer to a list of WordCount structs
 * @param word   Pointer to the word to be added into the list
 * 
 * @return void
 */
void add_word(WordCount **wclist, char *word);

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
void combine_lists(WordCount **wcListSource, WordCount **wcListExtension);

/**
 * Print words and their counts to a file stream.
 * 
 * @param wcHead       Pointer to the head of the WordCount list
 * @param outputStream Pointer to the output file stream
 * 
 * @return void
 */
void fprint_words(WordCount *wchead, FILE *ofile);

/**
 * Comparator to sort list by frequency, use alphabetical order as a secondary order.
 * 
 * @param wc1 The first WordCount operand
 * @param wc2 The second WordCount operand
 * 
 * @return bool Whether the first WordCount comes before the second WordCount
 */
bool wordcount_less(const WordCount *wc1, const WordCount *wc2);

/**
 * Sort a WordCount list in place, by the count of each word. Alphabetical
 * order of the words wil be used as a secondary order.
 * 
 * @param wclist Pointer to the list of WordCount structs
 * @param less   A comparator function that returns whether the first
 *               WordCount is less than the second
 * 
 * @return void The WordCountList is sorted in place
 */
void wordcount_sort(WordCount **wclist, bool less(const WordCount *, const WordCount *));


#endif /* word_count_h */
