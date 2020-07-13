/*
 * You may NOT modify this file. Any changes you make to this file will not
 * be used when grading your submission.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#define NUM_THREADS     4
int common = 162;
char *somethingshared;

void *threadfun(void *thread_id) {
  /**
   * threadid is an 8-byte long casted as a (void *) and is the thread ID;
   * tid copies the value of threadid, and &tid gives the address of tid;
   * &common gives the address of common to show whether it's the same
   * between threads;
   * somethingshared is a char pointer and is the address of the string on
   * heap;
   * somethingshared + tid gives the substring starting at index[tid]
   */
  long tid = (long) thread_id;
  /**
   * This prints:
   * Thread #<thread_id> stack: <local thread_id address> common: <common address> <common value> tptr: <thread_id>
   * <heap string address>: <substring starting at the thread_id index>
   */
  printf(
    "Thread #%lx stack: %lx common: %lx (%d) tptr: %lx\n",
    tid,
	  (unsigned long) &tid,
	  (unsigned long) &common,
    common++,
	  (unsigned long) thread_id
  );
  printf("Exiting with %lx: %s\n", (unsigned long) somethingshared, somethingshared + tid);
  pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
  int rc;
  long t;
  int nthreads = NUM_THREADS;
  char *targs = strcpy(malloc(100000),"I am on the heap.");

  if (argc > 1) {
    nthreads = atoi(argv[1]);
  }

  pthread_t threads[nthreads];

  /**
   * This prints:
   * Main stack: <main thread_id address>, common: <common address> <common value>
   * main: creating thread <thread_id>
   * 
   * Note: main thread_id address will be different from thread-local thread_id address,
   * because the thread_id will be passed by value.
   */
  printf
    ("Main stack: %lx, common: %lx (%d)\n",
	  (unsigned long) &t,
	  (unsigned long) &common,
    common
  );
  puts(targs);
  somethingshared = targs;
  for(t = 0; t < nthreads; t++) {
    printf("main: creating thread %ld\n", t);
    rc = pthread_create(&threads[t], NULL, threadfun, (void *)t);
    if (rc) {
      printf("ERROR; return code from pthread_create() is %d\n", rc);
      exit(-1);
    }
  }

  /* Last thing that main() should do */
  pthread_exit(NULL);
}

