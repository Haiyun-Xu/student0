/*

  Word Count using dedicated lists

*/

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

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "word_count.h"

/* The maximum length of each word in a file */
#define MAX_WORD_LEN 64

/**
 * Extract the next word from the input file stream, and store it in the
 * given c-string. If no word is found, return an empty c-string ("\0").
 * 
 * A word is a sequence of contiguous alphabetical characters. If the
 * sequence exceeds the wordSize, this function will write an empty c-string
 * to word.
 * 
 * Therefore, if an error ocurred during this function, then strlen(word) == 0
 * and !feof(inputFileStream).
 * 
 * @param file     Pointer to the input file stream
 * @param word     Pointer to the writable c-string
 * @param wordSize Maximum size of the given c-string, excluding the terminating null-char, 
 */
void extract_word(FILE *inputFileStream, char *word, int wordSize) {
  char *functionName = "extract_word";
  if (inputFileStream == NULL || word == NULL) {
    return;
  } else if (ferror(inputFileStream)) {
    fprintf(
      stderr,
      "Error from %s: an error (%d) exists in the given file stream.\n",
      functionName,
      ferror(inputFileStream)
    );
    return;
  }

  char character;
  int wordPosition = 0;
  bool wordFound = false;

  // iterate until either 1) a word is found, or 2) EOF is encountered
  while (!wordFound && !feof(inputFileStream)) {
    // get the next character from the file stream
    character = (char) fgetc(inputFileStream);
    if (ferror(inputFileStream)) {
      fprintf(
        stderr,
        "Error from %s: an error (%d) occurred while getting character from the file stream.\n",
        functionName,
        ferror(inputFileStream)
      );
      return;
    }

    /**
     * if the returned character was EOF, the position indicator has
     * reached the end of file, and there are no more words;
     * else if the returned character IS NOT an alphabet, skip it and
     * continue onto the next word;
     * otherwise, extract the word that starts from this character;
     */ 
    if (feof(inputFileStream)) {
      word[0] = '\0';
      return;
    } else if (!isalpha(character)) {
      continue;
    } else {
      // exit from the outer loop when the word is extracted
      wordFound = true;
      
      word[0] = tolower(character);
      wordPosition = 1;

      // extract the word starting at the current position
      while (!feof(inputFileStream)) {
        character = (char) fgetc(inputFileStream);
        if (ferror(inputFileStream)) {
          fprintf(
            stderr,
            "Error from %s: an error (%d) occurred while getting character from the file stream.\n",
            functionName,
            ferror(inputFileStream)
          );
          word[0] = '\0';
          return;
        }

        /**
         * if the returned character was EOF or not an alphabet, the position
         * indicator has reached the end of the word, so terminate the
         * word and exit from this loop;
         * else if the length of the word has exceeded maximum wordSize,
         * then the file has violated the limit;
         * otherwise, append the returned character to the word, and continue
         * onto the next character;
         */
        if (!isalpha(character)) {
          word[wordPosition] = '\0';
          return;
        } else if (wordPosition >= wordSize) {
          /**
           * this word has exceeded the wordSize limit and would overflew
           * the word array, so print an error message
           */
          fprintf(
            stderr,
            "Error from %s: character sequence in the file stream exceeds the maximum %d characters limit.\n",
            functionName,
            wordSize
          );
          word[0] = '\0';
          return;
        } else {
          word[wordPosition] = tolower(character);
          wordPosition++;
        }
      }
    }
  }
}

/**
 * Returns the total number of words found in a file. All words should
 * be converted to lower-case and be treated non case-sensitively.
 * 
 * @param file A file pointer to the input file stream.
 * 
 * @return int The number of words in the file. Or -1 if an error occurred.
 */
int num_words(FILE* file) {
  char *functionName = "num_words";
  // validate parameters
  if (file == NULL) {
    return 0;
  } else if (ferror(file)) {
    fprintf(
      stderr,
      "Error from %s: an error (%d) exists in the given file stream.\n",
      functionName,
      ferror(file)
    );
    return -1;
  }

  int wordSize = MAX_WORD_LEN, numOfWords = 0;
  // any return beyond this point should first free the word memory
  char *word = (char*) malloc(sizeof(char) * (wordSize + 1));

  while (!feof(file)) {
    // extract every single word till the EOF
    extract_word(file, word, wordSize);

    /*
     * if an empty string is returned:
     *  if the position indicator has reached the EOF, all words have been
     *  extracted, so exit the loop;
     *  otherwise, extract_word() encountered an error, so abort counting;
     * if a non-empty string is returned:
     *  if its length exceeds the maximum word length limit, then the file
     *  has a word that exceeds the maximum word length limit, so abort
     *  the counting;
     *  otherwise, a valid word is found, so increment the counter;
     */
    if (strlen(word) == 0) {
      if (feof(file)) {
        break;
      } else {
        fprintf(
          stderr,
          "Error from %s: unable to extract the next word from the file stream.\n",
          functionName
        );
        numOfWords = -1;
        break;
      }
    } else if (strlen(word) > MAX_WORD_LEN) {
      fprintf(
          stderr,
          "Error from %s: the given file stream contains a character sequence whose length exceeds the maximum limit of %d characters.\n",
          functionName,
          MAX_WORD_LEN
        );
        numOfWords = -1;
        break;
    } else {
      numOfWords ++;
    }
  }

  free(word);
  return numOfWords;
}

/**
 * Given a file stream, extract and add each word in the file to the given
 * list of WordCounts. The given list of WordCounts is changed atomically:
 * if an error occurrs midway, the given list will not be changed.
 * 
 * @param wcList Pointer to the list of WordCounts
 * @param file   Pointer to the input file stream
 * 
 * @return void
 */
void count_words(WordCount **wcList, FILE *file) {
  char *functionName = "count_words";
  // validate parameters
  if (wcList == NULL || file == NULL) {
    return;
  } else if (ferror(file)) {
    fprintf(
      stderr,
      "Error from %s: an error (%d) exists in the given file stream.\n",
      functionName,
      ferror(file)
    );
    return;
  }

  // count the words in the file separately in a new list first
  WordCount *wcNewList = NULL;
  init_words(&wcNewList);

  int wordSize = MAX_WORD_LEN;
  // any return beyond this point should first free the word memory
  char *word = (char*) malloc(sizeof(char) * (wordSize + 1));

  while (!feof(file)) {
    // extract every single word till the EOF
    extract_word(file, word, wordSize);

    /*
     * if an empty string is returned:
     *  if the position indicator has reached the EOF, all words have been
     *  extracted and counted, move on;
     *  otherwise, extract_word() encountered an error, so abort the counting;
     * if a non-empty string is returned:
     *  if its length is greater than the maximum word length, then the file
     *  has a word that exceeds the maximum word length limit, so abort
     *  the counting;
     *  otherwise,  a valid word is found, so add it to the new list;
     */
    if (strlen(word) == 0) {
      if (feof(file)) {
        break;
      } else {
        fprintf(
          stderr,
          "Error from %s: unable to extract the next word from the file stream.\n",
          functionName
        );
        clear_list(&wcNewList);
        free(word);
        return;
      }
    } else if (strlen(word) > MAX_WORD_LEN) {
      fprintf(
          stderr,
          "Error from %s: the given file stream contains a character sequence whose length exceeds the maximum limit of %d characters.\n",
          functionName,
          MAX_WORD_LEN
        );
        clear_list(&wcNewList);
        free(word);
        return;
    } else {
      add_word(&wcNewList, word);
    }
  }

  /*
   * all words in the file should have been counted into the new list,
   * so atomically combine the new list into the given list
   */
  combine_lists(wcList, &wcNewList);

  // free the new list and the word
  clear_list(&wcNewList);
  free(word);
  return;
}

/**
 * Displays a helpful message.
 * 
 * @param void
 * 
 * @return void
 */
static void display_help(void) {
	printf("Flags:\n"
	    "--count (-c): Count the total amount of words in the file, or STDIN if a file is not specified. This is default behavior if no flag is specified.\n"
	    "--frequency (-f): Count the frequency of each word in the file, or STDIN if a file is not specified.\n"
	    "--help (-h): Displays this help message.\n");
	return;
}

/**
 * Handle command line flags and arguments.
 * 
 * @param argc The number of arguments given. At least one argument is given
 * @param argv The array of argument strings. The first argument is always the name of the main file
 * 
 * @return int Returns 0 if program executed successfully, or a non-zero integer if an error occurred
 */
int main (int argc, char *argv[]) {

  // Count Mode (default): outputs the total number of words counted
  bool count_mode = true;
  // Freq Mode: outputs the frequency of each word
  bool freq_mode = false;
  
  int total_words = 0;
  FILE *infile = NULL;
  WordCount *wcList;

  // Variables for command line argument parsing
  int i;
  static struct option long_options[] = {
      {"count", no_argument, 0, 'c'},
      {"frequency", no_argument, 0, 'f'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}
  };

  // Sets flags
  while ((i = getopt_long(argc, argv, "cfh", long_options, NULL)) != -1) {
      switch (i) {
          case 'c':
              count_mode = true;
              freq_mode = false;
              break;
          case 'f':
              count_mode = false;
              freq_mode = true;
              break;
          case 'h':
              display_help();
              return 0;
      }
  }

  if (!count_mode && !freq_mode) {
    printf("Please specify a mode.\n");
    display_help();
    return 0;
  }

  /* Create the empty WordCount list */
  init_words(&wcList);

  if ((argc - optind) < 1) {
    // No input file specified, instead, read from STDIN instead.
    infile = stdin;
    if (count_mode) {
      total_words = num_words(infile);
      if (total_words == -1) {
        fprintf(stderr, "Error occurred while counting the total number of words.\n");
        return -1;
      }
    } else if (freq_mode) {
      count_words(&wcList, infile);
    }
  } else {
    /*
     * At least one file specified, where the first file can be found at
     * argv[optind], and the last file can be found at argv[argc-1]
     */ 
    for (int filenameIndex = optind; filenameIndex < argc; filenameIndex++) {
      infile = fopen(argv[filenameIndex], "r");
      if (infile == NULL) {
        perror(strcat("Failed to open file with the name ", argv[filenameIndex]));
        return -1;
      }
      
      if (count_mode) {
        int numOfWordsInFile = num_words(infile);
        fclose(infile);

        if (numOfWordsInFile == -1) {
          fprintf(stderr, "Error occurred while counting the total number of words.\n");
          return -1;
        } else {
          total_words += numOfWordsInFile;
        }
      } else if (freq_mode) {
        count_words(&wcList, infile);
        fclose(infile);
      }
    }
  }

  if (count_mode) {
    printf("The total number of words is: %i\n", total_words);
  } else {
    printf("The frequencies of each word are: \n");
    wordcount_sort(&wcList, wordcount_less);
    fprint_words(wcList, stdout);
  }

  clear_list(&wcList);
  return 0;
}
