#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "wq.h"
#include "http_helper.h"
#include "server_signal.h"
#include "server_config.h"

/**
 * Read all the content from the file descriptor into the buffer.
 * 
 * @param fd The file descriptor
 * @param bufferPtr Pointer to the buffer, terminated with a null-char
 * 
 * @return The number of bytes read into the buffer
 */
int read_all(int fd, char **bufferPtr) {
  // edge cases
  if (fd < 0 || bufferPtr == NULL) {
    return 0;
  }

  int bufferSize = INITIAL_BUFFER_SIZE;
  char *buffer = (char *) malloc(bufferSize);
  int bytesRead = 0, totalBytesRead = 0, bytesLeftInBuffer = bufferSize - totalBytesRead;

  // iteratively read the fd content into the buffer, until no more bytes are read
  do {
    totalBytesRead += bytesRead;

    // if the buffer has become full, double the buffer size
    if (totalBytesRead == bufferSize) {
      bufferSize *= 2;
      char *tempPtr = (char *) realloc(buffer, bufferSize);

      // if we failed to expand the buffer, end the function
      if (tempPtr == NULL) {
        fprintf(stderr, "Failed to allocate enough memory to buffer\n");
        free(buffer);
        *bufferPtr = NULL;
        return 0;

        // else, use the new buffer
      } else {
        buffer = tempPtr;
      }
    }

    bytesLeftInBuffer = bufferSize - totalBytesRead;
    bytesRead = read(fd, buffer + totalBytesRead, bytesLeftInBuffer);

    // if the read failed, free the buffer and abort
    if (bytesRead == -1) {
      perror("Failed to read from file descriptor: ");
      free(buffer);
      *bufferPtr = NULL;
      return 0;
    }
  } while (bytesRead > 0);

  // terminate the buffer with a null-char
  buffer[totalBytesRead] = '\0';
  *bufferPtr = buffer;
  return totalBytesRead;
}

/**
 * Write all the content from the buffer into the file descriptor.
 * NOTE: this function does not free the buffer
 * 
 * @param fd The file descriptor
 * @param buffer The buffer, terminated with a null-char
 * @param bufferSize Size of the buffer in bytes
 * 
 * @return The number of bytes written into the file descriptor
 */
int write_all(int fd, char *buffer, int bufferSize) {
  // edge cases
  if (fd < 0 || buffer == NULL || bufferSize < 0) {
    return 0;
  }

  int bytesLeftToWrite = bufferSize;
  int bytesWritten = 0, totalBytesWritten = 0;

  do {
    bytesWritten = write(fd, buffer, bytesLeftToWrite);
    // if the write failed, return -1
    if (bytesWritten == -1) {
      perror("Failed to write to file description: ");
      return bytesWritten;
    }

    totalBytesWritten += bytesWritten;
    bytesLeftToWrite = bufferSize - totalBytesWritten;
  } while (bytesLeftToWrite > 0);
  
  return totalBytesWritten;
}

/**
 * Shut down the connection and close the socket.
 * 
 * @param socketFD The socket file descriptor
 */
void close_socket(int socketFD) {
  // edge cases
  if (socketFD < 0) {
    return;
  }

  shutdown(socketFD, SHUT_RDWR);
  close(socketFD);
  return;
}

/**
 * Return a failure response to the client.
 * 
 * @param connectSocketFD The connection socket file descriptor
 * @param httpCode The failure HTTP code
 */
void send_failure_response(int connectSocketFD, int httpCode) {
  http_start_response(connectSocketFD, httpCode);
  http_send_header(connectSocketFD, "Content-Type", "text/html");
  http_end_headers(connectSocketFD);
  close_socket(connectSocketFD);
  return;
}

/**
 * Serves the contents of the file stored at `path` to the client.
 * The file stored at `path` should exist.
 * 
 * @param connectSocketFD The connection socket file descriptor
 * @param path The path of the file to be served
 */
void serve_file(int connectSocketFD, char *path) {
  // edge cases
  if (connectSocketFD < 0 || path == NULL) {
    return;
  }

  char *buffer = NULL; /** TODO: free this */
  int fileDescriptor = open(path, O_RDONLY);
  int bufferSize = read_all(fileDescriptor, &buffer);
  close(fileDescriptor);

  /*
   * if we failed to read the entire file content into buffer, it's the
   * server's fault
   */
  if (bufferSize == 0) {
    return send_failure_response(connectSocketFD, 500);
  }

  /*
   * the C int (unsigned int) is 4 bytes and can express at most
   * 2,147,483,647 (4,294,967,295), which is 11 bytes (including the null-char)
   */
  char contentLength[11];
  sprintf(contentLength, "%d", bufferSize);

  // start sending the response line and header lines
  http_start_response(connectSocketFD, 200);
  http_send_header(connectSocketFD, "Content-Type", http_get_mime_type(path));
  http_send_header(connectSocketFD, "Content-Length", contentLength);
  http_end_headers(connectSocketFD);
  
  // send the response body
  if (write_all(connectSocketFD, buffer, bufferSize) != bufferSize) {
    fprintf(stderr, "Failed to write all content into response\n");
  }

  free(buffer);
  return;
}

/**
 * Serves the contents of the directory stored at `path` to the client.
 * The relative directory at `path` ("./XXX") should be valid.
 * 
 * @param connectSocketFD The connection socket file descriptor
 * @param path The path of the file to be served
 */
void serve_directory(int connectSocketFD, char *path) {
  // edge cases
  if (connectSocketFD < 0 || path == NULL) {
    return;
  }

  int bufferSize = INITIAL_BUFFER_SIZE, totalBytesCopied = 0;
  char *lineBuffer = (char *) malloc(bufferSize); /** TODO: free this */
  char *buffer = (char *) malloc(bufferSize); /** TODO: free this */
  DIR *directory = opendir(path);
  struct dirent *directoryEntry = NULL;

  do {
    /*
     * iteratively read an entry from the directory:
     *  if the entry is NULL and there's no error, there's no more entry in
     *  the directory; if the entry is NULL but there's an erorr, reply that
     *  there was a server error;
     */
    errno = 0;
    if ((directoryEntry = readdir(directory)) == NULL){
      if (errno) {
        perror("Failed to load directory content: ");
        send_failure_response(connectSocketFD, 500);
        free(buffer);
        free(lineBuffer);
        return;
      } else {
        break;
      }
    }

    /*
     * the directory entry should be in html format: `<a href="/path/filename">filename</a><br/>\r\n`.
     * `path` is always relative (./XXX), but the requested path must start
     * with a forward-slash, so we ommit the '.' here. 
     */
    char *subPath = directoryEntry->d_name;
    int lineLength = 0;

    /*
     * If the subPath is the current directory ("."), then the link would
     * point to "/path/.", which is interpretable by the server; if the
     * subPath is the parent directory (".."), then we must make sure that
     * the relative parent directory is accessible, and remove the ".."
     * from the link; otherwise the server won't be able to interpret it.
     * 
     * There is only one directory that can return a valid parent directory:
     * /hw4/www/my_documents. If that's what the relative `path` resolves to,
     * then we will return the link to the relative parent directory. Since
     * we already confirmed that `path` does not contain ".." and is indeed
     * a valid directory, we don't need to worry about the server's working
     * directory, and only need to consider the `path` itself.
     * 
     * There are two valid `path` cases:
     *  if SERVER_FILE_PATH = "." = /hw4 and `path` = "./www/my_documents",
     *  then the parent directory link should be "/www";
     *  if SERVER_FILE_PATH = "./www" = /hw4/www and `path` = "./my_documents",
     *  then the parent directory link should be "/".
     * If `path` is neither of these cases, we skip over the link to the
     * parent directory.
     */
    if (strcmp(subPath, "..") == 0) {
      if (strcmp(path, "./www/my_documents") == 0 || strcmp(path, "./my_documents") == 0) {
        if (strcmp(SERVER_FILE_PATH, ".") == 0) {
          lineLength = snprintf(lineBuffer, INITIAL_BUFFER_SIZE, "<a href=\"/www\">%s</a><br/>\r\n", subPath);
        } else if (strcmp(SERVER_FILE_PATH, "./www") == 0) {
          lineLength = snprintf(lineBuffer, INITIAL_BUFFER_SIZE, "<a href=\"/\">%s</a><br/>\r\n", subPath);
        }
      } else {
        continue;
      }
    } else {
      lineLength = snprintf(lineBuffer, INITIAL_BUFFER_SIZE, "<a href=\"%s/%s\">%s</a><br/>\r\n", path + 1, subPath, subPath);
    }

    /*
     * if the formatted line is incorrectly encoded or is longer than the
     * (buffer - null-char), reply that there was a server error
     */
    if (lineLength < 0 || lineLength >= INITIAL_BUFFER_SIZE) {
      fprintf(stderr, "Directory entry too long for the buffer\n");
      send_failure_response(connectSocketFD, 500);
      free(buffer);
      free(lineBuffer);
      return;
    }

    /*
     * if the (line + null-char) does not fit in the remaining space in
     * the buffer, we double the size of the buffer; the buffer does not
     * expand if (line + null-char) fit snuggly in the remaining space
     */
    if ((lineLength + 1) > (bufferSize - totalBytesCopied)) {
      bufferSize *= 2;
      char *tempPtr = (char *) realloc(buffer, bufferSize);

      // if we failed to expand the buffer, end the function
      if (tempPtr == NULL) {
        fprintf(stderr, "Failed to allocate enough memory to buffer\n");
        send_failure_response(connectSocketFD, 500);
        free(buffer);
        free(lineBuffer);
        return;
        // else, use the new buffer
      } else {
        buffer = tempPtr;
      }
    }

    // copy the line into the buffer
    memcpy(buffer + totalBytesCopied, lineBuffer, lineLength);
    totalBytesCopied += lineLength;
  } while (true);
  buffer[totalBytesCopied] = '\0';

  /*
   * the C int (unsigned int) is 4 bytes and can express at most
   * 2,147,483,647 (4,294,967,295), which is 11 bytes (including the null-char)
   */
  char contentLength[11];
  sprintf(contentLength, "%d", totalBytesCopied);

  http_start_response(connectSocketFD, 200);
  http_send_header(connectSocketFD, "Content-Type", http_get_mime_type(".html"));
  http_send_header(connectSocketFD, "Content-Length", contentLength);
  http_end_headers(connectSocketFD);
  
  if (write_all(connectSocketFD, buffer, totalBytesCopied) != totalBytesCopied) {
    fprintf(stderr, "Failed to write all content into response\n");
  }

  free(buffer);
  free(lineBuffer);
  return;
}

/**
 * Reads an HTTP request from the connection socket, and writes an HTTP
 * response containing:
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 * Closes the connection socket when finished.
 * 
 * @param connectSocketFD The connection socket file descriptor
 */
void handle_files_request(int connectSocketFD) {
  // edge cases
  if (connectSocketFD < 0) {
    return;
  }

  struct http_request *request = http_request_parse(connectSocketFD);
  /*
   * if the request cannot be parsed or if the path does not start with '/',
   * then a bad request was received
   */
  if (request == NULL || request->path[0] != '/') {
    return send_failure_response(connectSocketFD, 400);
    /*
     * else if the requested path ever returns to the parent directory,
     * then we forbid the request from accessing that file
     */
  } else if (strstr(request->path, "..") != NULL) {
    return send_failure_response(connectSocketFD, 403);
    /*
     * else, check if the method and path are valid, and maybe serve the path
     */
  } else {
    // remove the trailing forward slash, and make the path relative
    if (request->path[strlen(request->path) - 1] == '/') {
      request->path[strlen(request->path) - 1] = '\0';
    }
    char *path = malloc(1 + strlen(request->path) + 1); /** TODO: free this */
    path[0] = '.';
    memcpy(path + 1, request->path, strlen(request->path) + 1);

    // check if the method is valid
    if (strcmp("GET", request->method) != 0) {
      send_failure_response(connectSocketFD, 405);
    } else {
      struct stat fileStatus;
      /*
       * if we failed to retrieve the file status, then reply that the path was
       * not found
       */ 
      if (stat(path, &fileStatus) == -1) {
        fprintf(stderr, "Failed to find path: %s\n", path);
        perror(NULL);
        send_failure_response(connectSocketFD, 404);
        /*
         * else if the path is a directory, either serve the index.html file
         * (if it exists), or reply that the path was not found
         */
      } else if (S_ISDIR(fileStatus.st_mode)) {
        // construct the path of the index file
        char *indexFileName = "/index.html";
        int totalLength = strlen(path) + strlen(indexFileName) + 1;
        char *indexFilePath = (char *) malloc(totalLength); /** TODO: free this */
        snprintf(indexFilePath, totalLength, "%s/index.html", path);

        struct stat indexFileStatus;
        /*
         * if there is an index.html file in the path, serve it
         */
        if (stat(indexFilePath, &indexFileStatus) != -1 && S_ISREG(indexFileStatus.st_mode)) {
          serve_file(connectSocketFD, indexFilePath);
          /*
           * else, there is no index.html in the path, so serve a list of
           * links to the subdirectories in the path
           */
        } else {
          serve_directory(connectSocketFD, path);
        }
        free(indexFilePath);
        /*
         * else if the path is a regular file, serve it
         */
      } else if (S_ISREG(fileStatus.st_mode)) {
        serve_file(connectSocketFD, path);
        /*
         * else, the requested path was not a valid file or directory,
         * so reply that the path was not found
         */
      } else {
        fprintf(stderr, "Path is not directory or file: %s\n", path);
        perror(NULL);
        send_failure_response(connectSocketFD, 404);
      }
    }
    free(path);
  }

  close_socket(connectSocketFD);
  return;
}

/**
 * The struct used as the argument to relay_communication().
 */
struct socket_pair {
  int sourceSocketFD;
  int targetSocketFD;
};

/**
 * Returns whether the connection on the given socket is still alive.
 * 
 * @param socketFD A socket file descriptor
 * 
 * @return Returns 0 if connection is alive, or -1 and set errno if not
 */
int is_connection_alive(int socketFD) {
  char temp = '\0';
  // write nothing into the socket, but check if the connection is closed
  int result = write(socketFD, &temp, 0);
  return result;
}

/**
 * Continuous forward client requests to remote server.
 * 
 * If we don't parse the client request, we won't need to reconstruct it.
 * That means we can just faithfully forward the request to remote, by simple
 * read, buffer, and write.
 * 
 * Requirement:
 * - if neither client nor the remote closes their connection, then the
 * server will continue to proxy indefinitely;
 * - whenever one of the two connections is closed, the server will detect
 * the connection closure and close both of its sockets, thereby closing
 * the other connection;
 * 
 * Specification:
 * - whenever a connection is closed by a peer, the thread reading the socket
 * will be pre-empted and unblocked;
 * - whenever a connection is closed by a server thread, other server threads
 * blocked by a read on that connection's socket will not be pre-empted;
 * - therefore, a solution to the above issue is to always make non-blocking
 * read calls;
 * 
 * Solution:
 * - to make non-blocking read calls, use the MSG_DONTWAIT flag on recv();
 * - to detect a closed connection at read time, attempt a 0-byte dummy
 * write on the socket;
 * 
 * @param argument Pointer to the argument struct socket_pair
 */
void *relay_communication(void *argument) {
  struct socket_pair *socketPair = (struct socket_pair *) argument;
  int sourceSocketFD = socketPair->sourceSocketFD;
  int targetSocketFD = socketPair->targetSocketFD;
  
  int bufferSize = INITIAL_BUFFER_SIZE;
  char *buffer = (char *) malloc(bufferSize); /** TODO: free this */
  int bytesRead, bytesWritten;

  // record the time at which the socket connections becomes alive
  time_t connectionStartTime;
  time(&connectionStartTime);

  /*
   * continuously read from the source and write to the target, and break
   * when either socket is closed
   */
  while (true) {
    /*
     * close both socket connections if they are older than how long the
     * sever wants to keep them alive
     */
    if (difftime(time(NULL), connectionStartTime) > CONNECTION_TTL) {
      printf("Connections older than TTL; closing connections\n");
      break;
    }

    /*
     * if a target socket connection is closed, the write() on that socket
     * can detect the closure; but if the source socket connection is closed,
     * we need to attempt a fake write() on the source socket to detect any
     * potential closure
     */
    if (is_connection_alive(sourceSocketFD) == -1) {
      if (errno != EPIPE && errno != EBADF) {
        perror("Error when attempting to read from socket: ");
      }
      break;
    }

    bytesRead = recv(sourceSocketFD, buffer, bufferSize, MSG_DONTWAIT);
    if (bytesRead == -1) {
      /*
       * if recv() returns -1, there are three scenarios:
       * 1) it returned because the connection is idle and there's no data
       * in the socket. This case is not an error, but the thread should
       * check the status of the other connection and see if the transaction
       * can be terminated;
       * 2) it returned and set errno to EBADF, which means other threads
       * have closed this socket, thereby signaling that the transaction should
       * terminate, and the current thread should exit;
       * 3) it returned and set errno to some other error, which means that
       * an error occurred, and the current thread should abort and exit;
       */
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // no-opt
      } else if (errno == EBADF) {
        break;
      } else {
        perror("Failed to read from socket: ");
        break;
      }
    }

    // detect target socket connection closure
    if (is_connection_alive(targetSocketFD) == -1) {
      if (errno != EPIPE && errno != EBADF) {
        perror("Error when attempting to write to socket: ");
      }
      break;
    }

    /*
     * if a target socket connection is closed, the write() on that socket
     * can detect the closure
     */
    bytesWritten = write_all(targetSocketFD, buffer, bytesRead);
    if (bytesWritten == -1) {
      /*
       * if write_all() returns -1, there are three scenarios:
       * 1) it returned and set errno to EPIPE, which means the connection
       * was closed by the peer socket, and therefore the transaction has
       * ended, and the current thread should close both sockets;
       * 2) it returned and set errno to EBADF, which means other threads
       * have closed this socket, thereby signaling that the transaction should
       * terminate, and the current thread should exit;
       * 3) it returned and set errno to some other error, which means that
       * an error occurred, and the current thread should abort and exit;
       */
      if (errno != EPIPE && errno != EBADF) {
        perror("Failed to write to socket: ");
      }
      break;
    }
  }

  /*
   * the thread that received the EPIPE should close both sockets, thereby
   * notifying the other thred to terminate
   */
  close_socket(sourceSocketFD);
  close_socket(targetSocketFD);
  free(buffer);
  return NULL;
}

/**
 * Opens a connection to the proxy target (hostname=SERVER_PROXY_HOSTNAME and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target_fd. HTTP requests from the client (fd) should be sent to the
 * proxy target (target_fd), and HTTP responses from the proxy target (target_fd)
 * should be sent to the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 *
 *   Closes client socket (fd) and proxy target fd (target_fd) when finished.
 * 
 * @param connectSocketFD The connection socket file descriptor
 */
void handle_proxy_request(int connectSocketFD) {
  // edge cases
  if (connectSocketFD < 0) {
    return;
  }

  /*
  * The code below does a DNS lookup of SERVER_PROXY_HOSTNAME and 
  * opens a connection to it.
  */
  // use DNS to resolve the proxy remote's IP address
  struct hostent *remoteDnsEntry = gethostbyname2(SERVER_PROXY_HOSTNAME, AF_INET);
  if (remoteDnsEntry == NULL) {
    fprintf(stderr, "Cannot resolve IP address for host: %s\n", SERVER_PROXY_HOSTNAME);
    send_failure_response(connectSocketFD, 502);
    close_socket(connectSocketFD);
    exit(ENXIO);
  }
  char *resolvedRemoteAddress = remoteDnsEntry->h_addr;

  // setup the IPv4 remote address struct
  struct sockaddr_in remoteAddress;
  bzero((char *)&remoteAddress, sizeof(remoteAddress));
  remoteAddress.sin_family = AF_INET;
  remoteAddress.sin_port = htons(SERVER_PROXY_PORT);
  memcpy(&(remoteAddress.sin_addr), resolvedRemoteAddress, sizeof(remoteAddress.sin_addr));

  // Create an IPv4 TCP socket to communicate with the proxy remote.
  int remoteSocketFD = socket(PF_INET, SOCK_STREAM, 0);
  if (remoteSocketFD == -1) {
    perror("Failed to create a new socket: ");
    send_failure_response(connectSocketFD, 502);
    close_socket(connectSocketFD);
    exit(errno);
  }

  if (connect(remoteSocketFD, (struct sockaddr*) &remoteAddress, sizeof(remoteAddress)) == -1) {
    perror("Failed to connect to remote server: ");
    close_socket(remoteSocketFD);
    send_failure_response(connectSocketFD, 502);
    close_socket(connectSocketFD);
    return;
  }

  // initialize the thread arguments
  struct socket_pair socketPairs[2];
  socketPairs[0].sourceSocketFD = connectSocketFD;
  socketPairs[0].targetSocketFD = remoteSocketFD;
  socketPairs[1].sourceSocketFD = remoteSocketFD;
  socketPairs[1].targetSocketFD = connectSocketFD;

  // spin up the threads
  pthread_t threads[2];
  int numOfThreads = 2, threadNum = 0, result = 0;
  for (threadNum = 0; threadNum < numOfThreads; threadNum++) {
    if ((result = pthread_create(&(threads[threadNum]), NULL, relay_communication, &(socketPairs[threadNum]))) != 0) {
      fprintf(stderr, "Failed to create proxy thread %d: error number %d\n", threadNum, result);
      close_socket(remoteSocketFD);
      send_failure_response(connectSocketFD, 502);
      close_socket(connectSocketFD);
      return;
    }
    printf("Started proxy thread %d\n", threadNum);
  }

  // wait for the threads to return
  for (threadNum = 0; threadNum < numOfThreads; threadNum++) {
    if((result = pthread_join(threads[threadNum], NULL)) != 0) {
      fprintf(stderr, "Failed to join with proxy thread %d: error number %d\n", threadNum, result);
    }
    printf("Stopped proxy thread %d\n", threadNum);
  }
  return;
}

#ifdef POOLSERVER

/**
 * The task loop in which handler threads run, until the main server thread
 * terminates. Each thread should block (instead of busy waiting) until a
 * new request has been received. When the main server thread accepts a new
 * connection, a handler thread should be dispatched to handle the client request.
 * 
 * @param request_handler The request_handler_func that the handler should use
 */
void *handle_clients(void *request_handler) {
  request_handler_func requestHandler = (request_handler_func) request_handler;
  /*
   * Detach the current thread, so that the thread frees its own memory on
   * completion, since the main thread won't be joining on it.
   */
  pthread_detach(pthread_self());

  int connectSocketFD;
  while (true) {
    /*
     * since all that the current thread gets is a file descriptor (meaning
     * that there's no additional states to indicate whether the thread can
     * start handling the client connection), there's no need to place the
     * blocking call in a loop and guard the loop with a states check
     */
    connectSocketFD = wq_pop(&WORK_QUEUE);
    requestHandler(connectSocketFD);
  }
  
  return NULL;
}

/**
 * Creates a number of threads and initializes the work queue.
 * 
 * @param numThreads The number of threads to create
 * @param requestHandler The request handler function each thread will use
 */
void init_thread_pool(int numThreads, request_handler_func requestHandler) {
  // edge cases
  if (numThreads == 0) {
    return;
  }

  /*
   * we don't need to identify each of the threads, so all threads can use
   * a single pthread_t
   */
  pthread_t thread;
  int result;
  for (int threadNum = 0; threadNum < numThreads; threadNum++) {
    if ((result = pthread_create(&thread, NULL, handle_clients, requestHandler)) != 0) {
      fprintf(stderr, "Failed to create thread pool: error number %d\n", result);
      exit(EXIT_FAILURE);
    }
    printf("Spawned handler thread %d out of %d\n", threadNum+1, numThreads);
  }

  wq_init(&WORK_QUEUE);
  return;
}

#endif

/**
 * The struct used as the argument to handle_request().
 */
struct request_task {
  request_handler_func handler;
  int connectSocketFD;
};

/**
 * Handle a request as a separate thread, using given handler and parameters.
 * 
 * @param argument Pointer to the argument struct request_task
 */
void *handle_request(void *argument) {
  /*
   * Detach the current thread, so that the thread frees its own memory on
   * completion, since the main thread won't be joining on it.
   */
  pthread_detach(pthread_self());

  struct request_task *task = (struct request_task *) argument;
  task->handler(task->connectSocketFD);
  
  free(task);
  printf("Exiting handler thread\n");
  return NULL;
}

/**
 * Exit the process after printing the program usage.
 */
void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

/**
 * Parse the commands and initialize server configs.
 * If arguments were not 
 * 
 * @param argc The number of arguments
 * @param argv The list of arguments
 */
void parse_commands(const int argc, char **argv) {
  // edge cases
  if (argc <= 1 || argv == NULL) {
    fprintf(stderr, "Must provide program arguments\n");
    exit_with_usage();
  }

  // iteratively parse and interpret the commands
  for (int i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      REQUEST_HANDLER = handle_files_request;

      SERVER_FILE_PATH = argv[++i];
      if (SERVER_FILE_PATH == NULL) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }

    } else if (strcmp("--proxy", argv[i]) == 0) {
      REQUEST_HANDLER = handle_proxy_request;

      char *proxyAddress = argv[++i];
      if (proxyAddress == NULL) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      // if the remote host port is not specified, use port 80 as default
      char *colonPointer = strchr(proxyAddress, ':');
      if (colonPointer != NULL) {
        *colonPointer = '\0';
        SERVER_PROXY_HOSTNAME = proxyAddress;
        SERVER_PROXY_PORT = atoi(colonPointer + 1);
      } else {
        SERVER_PROXY_HOSTNAME = proxyAddress;
        SERVER_PROXY_PORT = 80;
      }

    } else if (strcmp("--port", argv[i]) == 0) {
      char *serverPortString = argv[++i];
      if (serverPortString == NULL || (SERVER_PORT = atoi(serverPortString)) < 1) {
        fprintf(stderr, "Expected a valid port number after --port\n");
        exit_with_usage();
      }

    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char *numThreadsString = argv[++i];
      if (numThreadsString == NULL || (NUM_THREADS = atoi(numThreadsString)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }

    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();

    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();

    }
  }

  // if neither of the required arguments were specified, then exit
  if (REQUEST_HANDLER == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

#ifdef POOLSERVER
  // if running a server with thread pool, there must be at least one thread in the pool
  if (NUM_THREADS < 1) {
    fprintf(stderr, "Please specify \"--num-threads [N]\"\n");
    exit_with_usage();
  }
#endif

  return;
}

/**
 * Change the server process's working directory.
 * 
 * NOTE: there is no internal restriction on which directories can be
 * accessed, but the server should only serve files within the
 * "/home/vagrant/code/personal/hw4/www" directory. Since the server is
 * already in the hw4 directory, the server starter command must always
 * specify either ./www or ./www/my_documents as the starting directory
 */
void change_working_directory() {
  chdir(SERVER_FILE_PATH);
  if (errno) {
    fprintf(stderr, "Unable to serve from %s\n", SERVER_FILE_PATH);
    perror(NULL);
    exit(EXIT_FAILURE);
  }
}

/**
 * Setup the IPv4 server socket.
 * 
 * @param serverSocketFD Pointer to the server socket file descriptor
 */
void setup_server_socket(int *serverSocketFD) {
  /*
   * The server socket should 1) follow the IPv4 protocol family standard and
   * bind to an IPv4 address, 2) send data in byte stream; 3) use the default
   * protocol in the IPv4 protocol family, i.e. the TCP protocol
   */
  *serverSocketFD = socket(PF_INET, SOCK_STREAM, 0);
  if (*serverSocketFD == -1) {
    perror("Failed to create server socket: ");
    exit(errno);
  }

  /*
   * A socket address that has been bound will be unavailable for some time
   * after the socket is closed, but asserting the SO_REUSEADDR option will make
   * the socket address immediately re-usable.
   */
  int reuseAddress = 1;
  if (setsockopt(*serverSocketFD, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress)) == -1) {
    perror("Failed to set server socket options: ");
    exit(errno);
  }

  return;
}

/**
 * Setup the IPv4 server address.
 * 
 * @param serverAddress Pointer to the IPv4 server address struct
 */
void setup_server_address(struct sockaddr_in *serverAddress) {
  // zero out the serverAddress struct
  bzero((char *)serverAddress, sizeof(*serverAddress));

  /*
   * The socket belongs to the IPv4 protocol family, so it also belongs to the
   * IPv4 address family, and therefore uses the protocol + IPv4 address + port
   * number trio to represent the public address of the socket. Since this
   * server doesn't care via which network interface the connection comes
   * (WiFi, localhost, etc.), the socket address is set to arbitrary via
   * INADDR_ANY.
   */
  serverAddress->sin_family = AF_INET;
  // converts the unsigned short from host byte order to network byte order
  serverAddress->sin_port = htons(SERVER_PORT);
  // socket address should be the same as the machine address
  serverAddress->sin_addr.s_addr = INADDR_ANY;
  
  return;
}

/**
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *serverSocketNum. For each accepted
 * connection, calls request_handler with the accepted fd number.
 * 
 * How requests are served:
 * 1) (usually random) socket on client host (192.168.162.1:4444) <==> known
 * socket on server host (192.168.162.162:8000);
 * 2) random socket on server host (10.0.2.15:52908) <==> known socket on
 * remote host (172.217.7.14:80 for google.com);
 * 
 * @param serverSocketFD Pointer to the server socket file descriptor
 * @param requestHandler The client connection handler function
 */
void serve_forever(int *serverSocketFD, void (*requestHandler)(int)) {
  // edge cases
  if (serverSocketFD == NULL || requestHandler == NULL) {
    fprintf(stderr, "Server socket and handler function not provided\n");
    exit(EXIT_FAILURE);
  }

  // setup server socket
  setup_server_socket(serverSocketFD);

  // setup server address
  struct sockaddr_in serverAddress;
  setup_server_address(&serverAddress);

  // bind the server socket to the server address
  if (bind(*serverSocketFD, (struct sockaddr *) &serverAddress, (socklen_t) sizeof(serverAddress)) == -1) {
    perror("Failed to bind server socket to server address: ");
    exit(errno);
  }

  // mark the server socket as a passive, connection-accepting socket
  listen(*serverSocketFD, SERVER_CONNECTION_BACKLOG_LENGTH);
  printf("Listening on port %d...\n", SERVER_PORT);

#ifdef POOLSERVER

  /* 
   * initialize the thread pool before the main server thread accepts
   * any client connection
   */
  init_thread_pool(NUM_THREADS, REQUEST_HANDLER);

#endif

  struct sockaddr_in clientAddress;
  size_t clientAddressLen = sizeof(clientAddress);
  int connectSocketFD;
  // variables for THREADSERVER only
  int handlerThreadNum = 0;

  // continuously accept connection requests and handle client connections
  while (true) {
    // block and wait for connection request through the server socket
    connectSocketFD = accept(*serverSocketFD, (struct sockaddr *) &clientAddress, (socklen_t *) &clientAddressLen);
    if (connectSocketFD == -1) {
      perror("Error accepting connection request");
      continue;
    } else {
      printf("Accepted connection request from %s and port %d\n", inet_ntoa(clientAddress.sin_addr), clientAddress.sin_port);
    }

#ifdef BASICSERVER
    /*
     * This version of the server uses a single-process, single-threaded
     * to handle the client request. When a client connection is accepted,
     * the main process sends a response to the client. During this time,
     * the server does not listen nor accept new connections. Only after
     * the client connection terminates can the server accept a new connection.
     */
    requestHandler(connectSocketFD);

#elif FORKSERVER
    /* 
     * This version of the server uses a separate child process to handle
     * the client request. When a client connection is accepted, a new
     * process is forked. The child process will send a response to the
     * client and then exit. During this time, the parent process should
     * continue listening and accepting connections.
     */
    pid_t processID;
    processID = fork();

    /*
     * if the parent failed to fork, cannot handle the client connection
     * and request, so stop the server. Otherwise, the parent process
     * continues to listen for new client connection and does not wait for
     * the child process
     */
    if (processID == -1) {
      perror("Failed to fork child process: ");
      send_failure_response(connectSocketFD, 500);
      close_socket(connectSocketFD);
      break;

      /*
       * the child process closes the server socket, takes care of the
       * client request, and shuts down the client connection. It only
       * closes the server socket without shutting down the connection,
       * because the parent process is still listening on it.
       */
    } else if (processID == 0) {
      close(*serverSocketFD);
      requestHandler(connectSocketFD);
      printf("Child process exiting\n");
      exit(EXIT_SUCCESS);
    }

#elif THREADSERVER
    /* 
     * This version of the server uses a separate thread to handle the client
     * request. When a client connection is accepted, a new thread is created
     * to send a response to the client. The main thread should continue to
     * listen and accept new connections, and will NOT join with the new thread.
     */

    /*
     * because the main thread will continue to run, the argument to the
     * new thread must be allocated on the heap (so as to not be overwritten).
     * Each new thread should take ownership of argument and free it before
     * terminating
     */
    struct request_task *task = (struct request_task *) malloc(sizeof(struct request_task));
    task->handler = requestHandler;
    task->connectSocketFD = connectSocketFD;

    /*
     * if handler thread creation failed, respond to the client with an
     * internal server error message and close the client connection, but
     * instead of exiting, continue to listen for new connection request
     */
    pthread_t handlerThread;
    int threadCreationResult;
    if ((threadCreationResult = pthread_create(&handlerThread, NULL, handle_request, task)) != 0) {
      fprintf(stderr, "Failed to create handler thread %d: error number %d\n", handlerThreadNum++, threadCreationResult);
      send_failure_response(connectSocketFD, 500);
      close_socket(connectSocketFD);
    } else {
      printf("Started handler thread %d\n", handlerThreadNum++);
    }

#elif POOLSERVER
    /*
     * This version of the server uses a pool of constantly running threads
     * to handle the client request. When a client connection is accepted, the
     * main server thread adds the client's socket into the work queue. A
     * handler thread in the thread pool will pick up the client socket, handle
     * the request, and send a response to the client.
     */
    wq_push(&WORK_QUEUE, connectSocketFD);

#endif
  }

  close_socket(*serverSocketFD);
}

/**
 * The entry point of the program.
 * 
 * @param argc The number of arguments
 * @param argv The list of arguments
 * 
 * @return Returns 0 on success, 1 on failure
 */
int main(int argc, char **argv) {
  handle_signals();

  // parse the commandline and populate the global variables with arguments
  parse_commands(argc, argv);

  // if the server is run in file mode, change the process's working directory
  if (REQUEST_HANDLER == handle_files_request) {
    change_working_directory();
  }

  serve_forever(&SERVER_SOCKET, REQUEST_HANDLER);

  return EXIT_SUCCESS;
}
