/**
 * This module contains global configuration and variables for the server,
 * and should be initialized to either invalid or initial values.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com
 * @copyright MIT
 */

#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include "wq.h"

typedef void (*request_handler_func) (int);

extern char *USAGE;

// the file descriptor number representing the server socket
extern int SERVER_SOCKET;

// the port at which the server is listening for connection requests
extern int SERVER_PORT;

// the path of the file requested
extern char *SERVER_FILE_PATH;

// the hostname of the remote requested
extern char *SERVER_PROXY_HOSTNAME;
extern int SERVER_PROXY_PORT;

// length of the server socket connection backlog
extern int SERVER_CONNECTION_BACKLOG_LENGTH;

// initial size of the buffer in bytes
extern int INITIAL_BUFFER_SIZE;

// close the socket connection after this amount of time in seconds
extern double CONNECTION_TTL;

// only used by poolserver
wq_t WORK_QUEUE;
extern int NUM_THREADS;

// the request handler function for the server process
extern request_handler_func REQUEST_HANDLER;

#endif /* SERVER_CONFIG_H */