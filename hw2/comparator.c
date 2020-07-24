/**
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @opyright MIT
 */

#define MAX_LINE_LENGTH 64

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>


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
 * Accepts two word count output files, and compare the results.
 */
int main(int argc, char* argv[]) {
    char answerLine[MAX_LINE_LENGTH];
    char resultLine[MAX_LINE_LENGTH];

    FILE *answer = fopen(argv[1], "r");
    FILE *result = fopen(argv[2], "r");

    char *answerPtr, *resultPtr;
    int answerCount, resultCount;
    bool mismatch = false;

    while (
        (answerPtr = fgets(answerLine, MAX_LINE_LENGTH, answer)) != NULL
        && (resultPtr = fgets(resultLine, MAX_LINE_LENGTH, result)) != NULL
    ) {
        answerCount = lineToWordAndCount(answerLine, &answerPtr);
        resultCount = lineToWordAndCount(resultLine, &resultPtr);

        if (strcmp(answerPtr, resultPtr) || answerCount != resultCount) {
            printf("Mismatch: answer's %s x%d != result's %s x%d.\n", answerPtr, answerCount, resultPtr, resultCount);
            mismatch = true;
        }

        free(answerPtr);
        free(resultPtr);

        // if (mismatch) break;
        // else continue;
    }
    if (!mismatch) printf("The answer and result are the same.\n");
    fclose(answer);
    fclose(result);
    return 0;
}