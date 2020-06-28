/**
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "word_count.h"
#include "word_helpers.h"

#define MAX_LINE_LENGTH 64

/**
 * Given a line/string, extract the word and its count. Point the given char**
 * to the word on heap, and return the count as an integer.
 * 
 * @param line A line/string containing the word and its count, in the
 *             "%8d\t%s\n" format
 * @param word A char* pointer, points to the word in the line when returned
 * 
 * @return void
 */
int lineToWordAndCount(const char* line, char** wordPtr) {
    // function configuration
    int wordStartIndex = 9;
    int countStrLength = 8;

    // edge cases
    if (line == NULL) return -1;

    // find the address of the new line character
    char *newLineAddress = strrchr(line, '\n');
    if (newLineAddress == NULL) return -1;

    // calculate the length of the word
    const char* firstCharOfWordAddress = line + wordStartIndex;
    int wordLength = newLineAddress - firstCharOfWordAddress;

    // extract the word into heap and point wordPtr to it
    (*wordPtr) = malloc(sizeof(char) * (wordLength + 1));
    if (*wordPtr == NULL) return -1;
    strncpy(*wordPtr, firstCharOfWordAddress, wordLength);
    (*wordPtr)[wordLength] = '\0';

    // extract the first 8 bytes of the line, convert it to count and return
    char countStr[9]; // countStrLength + 1
    strncpy(countStr, line, countStrLength);
    countStr[countStrLength] = '\0';
    return atoi(countStr);
}

/**
 * Given a line/string, extract the word and its count, and insert them as a
 * new word_count_t node into the given word_count_list_t.
 * 
 * @param line A line/string containing the word and its content, in the
 *             "%8d\t%s\n" format
 * @param wcList Pointer to the list of word_Count_t to add the new word to
 * 
 * @return void
 */
void lineToWordCount(const char* line, word_count_list_t* wcList) {
    // edge cases
    if (line == NULL) return;

    // extract the word and count from the line
    char* wordPtr = NULL;
    int count = lineToWordAndCount(line, &wordPtr);
    if (wordPtr == NULL || count <= 0) return;

    // build a new word_count_t node
    word_count_t *wordCountPtr = (word_count_t *) malloc(sizeof(word_count_t));
    wordCountPtr->count = count;
    wordCountPtr->word = wordPtr;

    // insert the new node into the list
    wordCountPtr->next = *wcList;
    (*wcList) = wordCountPtr;
    return;    
}

/**
 * Clear the word_count list by freeing all the heap memory allocated to it.
 * 
 * @param wcList Pointer to the word_count_t list
 * 
 * @return void
 */
void clear_list(word_count_list_t* wcList) {
  if (wcList == NULL) return;

  word_count_t *wc = (*wcList), *wcNext = NULL;
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
 * Take the stdout output from words.c, regenerate the word_count_list_t
 * data structure, and re-print it to stdout.
 * 
 * @param argc The number of arguments given
 * @param argv The list of pointers to argument strings
 * 
 * @return void
 */ 
int main(int argc, char* argv[]) {
    char line[MAX_LINE_LENGTH];
    word_count_list_t wcList = NULL;
    init_words(&wcList);

    // iteratively read line from stdin
    while (!feof(stdin)) {
        char* resultPtr = fgets(line, MAX_LINE_LENGTH, stdin);
        if (resultPtr == NULL) break;

        // add the new word to word_count_t list
        lineToWordCount(line, &wcList);
    }
    // if exited the loop because of error, free the list and return
    if (ferror(stdin)) {
        clear_list(&wcList);
        return 1;
    }

    // sort the word count nodes and print them
    wordcount_sort(&wcList, less_count);
    fprint_words(&wcList, stdout);
    
    clear_list(&wcList);
    return 0;
}