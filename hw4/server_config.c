/**
 * This module contains global configuration and variables for the server,
 * and should be initialized to either invalid or initial values.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com
 * @copyright MIT
 */

#include "server_config.h"

char *USAGE =
  "Usage: ./<server> --files some_directory/ [--port 8000 --num-threads 5]\n"
  "       ./<server> --proxy example.com:80 [--port 8000 --num-threads 5]\n";

// the file descriptor number representing the server socket
int SERVER_SOCKET = -1;

// the port at which the server is listening for connection requests
int SERVER_PORT = 8000;

// the path of the file requested
char *SERVER_FILE_PATH = NULL;

// the hostname of the remote requested
char *SERVER_PROXY_HOSTNAME = NULL;
int SERVER_PROXY_PORT = 80;

// length of the server socket connection backlog
int SERVER_CONNECTION_BACKLOG_LENGTH = 1024;

// initial size of the buffer in bytes
int INITIAL_BUFFER_SIZE = 1024;

// close the socket connection after this amount of time in seconds
double CONNECTION_TTL = 30.0;

// only used by poolserver
int NUM_THREADS = 5;

// the request handler function for the server process
request_handler_func REQUEST_HANDLER = NULL;