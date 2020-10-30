/**
 * This module contains error handling logic for the server.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "server_signal.h"

/**
 * Handle a signal that terminates the process.
 * Exits the process instead of returning.
 * 
 * @param signalNumber The received signal number
 * 
 * @return void
 */
void handle_terminate_signal(int signalNumber) {
  printf("Received signal %d: %s\n", signalNumber, strsignal(signalNumber));
  printf("Closing server socket %d\n", SERVER_SOCKET);

  // if the process is asked to terminate, attempt to close the server socket
  if (close(SERVER_SOCKET) < 0)
    perror("Failed to close server socket (ignoring)\n");
  exit(0);
}

/**
 * Handles received signals.
 * 
 * @return void
 */
void handle_signals() {
  /* 
   * SIGPIPE is emitted to the process if it calls write() on a pipe whose
   * read end has been closed, i.e. all file descriptors referring to the read
   * end are closed. We ignore this signal, so that write() fails with the EPIPE
   * errno.
   */
  signal(SIGPIPE, SIG_IGN);
  // handles a SIGINT that terminates the process
  signal(SIGINT, handle_terminate_signal);
}