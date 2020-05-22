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

/* Global data structure tracking the words encountered */
WordCount *word_counts = NULL;

/* The maximum length of each word in a file */
#define MAX_WORD_LEN 64

/**
 * Returns the total number of words found in a file.
 * 
 * A word is: 1) a sequence of contiguous alphabetical characters; 2)
 * sequence length should be greater than one, but no greater than the
 * maximum length; 3) all words should be converted to lower-case and
 * be treated non case-sensitively.
 * 
 * @param file A file pointer to the input file stream.
 * 
 * @return int The number of words in the file. Or -1 if an error occurred.
 * 
 * Useful functions: fgetc(), isalpha().
 */
int num_words(FILE* file) {
  char *functionName = "num_words";
  // check for edge cases
  if (file == NULL) {
    fprintf(stderr, "Error from %s: the file pointer given was NULL", functionName);
    return -1;
  } else if (ferror(file)) {
    fprintf(
      stderr,
      "Error from %s: an error (%d) exists in the given file stream",
      functionName,
      ferror(file)
    );
    return -1;
  }

  int numOfWords = 0;
  char character;
  // any return beyond this point should first free the word memory
  char *word = (char*) malloc(sizeof(char) * (MAX_WORD_LEN + 1));
  int wordPosition = 0;

  // iterate until EOF to extract all words
  while (!feof(file)) {

    // get the next character from the file stream
    character = (char) fgetc(file);
    if (ferror(file)) {
      fprintf(
        stderr,
        "Error from %s: an error (%d) occurred while getting character from the file stream",
        functionName,
        ferror(file)
      );
      free(word);
      return -1;
    }

    /**
     * if the returned character was EOF, the position indicator has
     * reached the end of file, and all words should have been found;
     * else if the returned character IS NOT an alphabet, skip it and
     * continue onto the next word;
     * otherwise, extract the word that starts from this character;
     */ 
    if (feof(file)) {
      break;
    } else if (!isalpha(character)) {
      continue;
    } else {
      word[0] = character;
      wordPosition = 1;

      // extract the word starting at the current position
      while (!feof(file)) {
        character = (char) fgetc(file);
        if (ferror(file)) {
          fprintf(
            stderr,
            "Error from %s: an error (%d) occurred while getting character from the file stream",
            functionName,
            ferror(file)
          );
          free(word);
          return -1;
        }

        /**
         * if the returned character was EOF or not an alphabet, the position
         * indicator has reached the end of the word, so exit from this loop;
         * else if the length of the word has reached the size of the array,
         * then then file violated the MAX_WORD_LEN limit;
         * otherwise, append the returned character to the word, and continue
         * onto the next character;
         */
        if (!isalpha(character)) {
          break;
        } else if (wordPosition >= MAX_WORD_LEN) {
          /**
           * this word has exceeded the MAX_WORD_LEN limit and overflew
           * the word array, an error code should be returned
           */
          fprintf(
            stderr,
            "Error from %s: character sequence in the file stream exceeds the maximum %d characters limit",
            functionName,
            MAX_WORD_LEN
          );
          free(word);
          return -1;
        } else {
          character = tolower(character);
          word[wordPosition] = character;
          wordPosition++;
        }
      }
      // add the terminating null-char to the word
      word[wordPosition] = '\0';
      // add this word to the total number if its length is valid
      int wordSize = strlen(word);
      if (wordSize > 1 && wordSize <= MAX_WORD_LEN)
        numOfWords++;
    }
  }

  free(word);
  return numOfWords;
}

/**
 * TODO:
 *
 * Given infile, extracts and adds each word in the FILE to `wclist`.
 * Useful functions: fgetc(), isalpha(), tolower(), add_word().
 */
void count_words(WordCount **wclist, FILE *infile) {
}

/**
 * Comparator to sort list by frequency, use alphabetical order as a secondary order.
 * 
 * @param wc1 The first WordCount operand
 * @param wc2 The second WordCount operand
 * 
 * @return bool Whether the first WordCount comes before the second WordCount
 */
static bool wordcount_less(const WordCount *wc1, const WordCount *wc2) {
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

// In trying times, displays a helpful message.
static int display_help(void) {
	printf("Flags:\n"
	    "--count (-c): Count the total amount of words in the file, or STDIN if a file is not specified. This is default behavior if no flag is specified.\n"
	    "--frequency (-f): Count the frequency of each word in the file, or STDIN if a file is not specified.\n"
	    "--help (-h): Displays this help message.\n");
	return 0;
}

/**
 * Handle command line flags and arguments.
 */
int main (int argc, char *argv[]) {

  // Count Mode (default): outputs the total amount of words counted
  bool count_mode = true;
  int total_words = 0;

  // Freq Mode: outputs the frequency of each word
  bool freq_mode = false;

  FILE *infile = NULL;

  // Variables for command line argument parsing
  int i;
  static struct option long_options[] =
  {
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
              return display_help();
      }
  }

  if (!count_mode && !freq_mode) {
    printf("Please specify a mode.\n");
    return display_help();
  }

  /* Create the empty data structure */
  init_words(&word_counts);

  if ((argc - optind) < 1) {
    // No input file specified, instead, read from STDIN instead.
    infile = stdin;
    total_words = num_words(infile);
    if (total_words == -1) {
      fprintf(stderr, "Error occurred while counting the total number of words");
      return -1;
    }
  } else {
    // At least one file specified. Useful functions: fopen(), fclose().
    // The first file can be found at argv[optind]. The last file can be
    // found at argv[argc-1].
    for (int filenameIndex = optind; filenameIndex < argc; filenameIndex++) {
      infile = fopen(argv[filenameIndex], "r");
      if (infile == NULL) {
        perror(strcat("Failed to open file with the name ", argv[filenameIndex]));
        return -1;
      }

      int numOfWordsInFile = num_words(infile);
      fclose(infile);

      if (numOfWordsInFile == -1) {
        fprintf(stderr, "Error occurred while counting the total number of words");
        return -1;
      }
      total_words += numOfWordsInFile;
    }
  }

  if (count_mode) {
    printf("The total number of words is: %i\n", total_words);
  } else {
    wordcount_sort(&word_counts, wordcount_less);

    printf("The frequencies of each word are: \n");
    fprint_words(word_counts, stdout);
}
  return 0;
}
