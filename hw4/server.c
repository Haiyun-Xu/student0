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
#include <unistd.h>

#include "server_config.h"
#include "server_signal.h"
#include "libhttp.h"
#include "wq.h"

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
        printf("Failed to allocate enough memory to file buffer\n");
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
  if (fd < 0 || buffer == NULL || bufferSize <= 0) {
    return 0;
  }

  int bytesLeftToWrite = bufferSize;
  int bytesWritten = 0, totalBytesWritten = 0;

  do {
    bytesWritten = write(fd, buffer, bytesLeftToWrite);
    // if the write failed, return the total number of written bytes and abort
    if (bytesWritten == -1) {
      perror("Failed to write to file description: ");
      return totalBytesWritten;
    }

    totalBytesWritten += bytesWritten;
    bytesLeftToWrite = bufferSize - totalBytesWritten;
  } while (bytesLeftToWrite > 0);
  
  return totalBytesWritten;
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
  close(connectSocketFD);
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

void serve_directory(int fd, char *path) {
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
  http_end_headers(fd);

  /* TODO: PART 3 */

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
          * else, there is no index.html in the directory, and we don't serve
          * directories yet, so reply that the path is not found
          */
        } else {
          send_failure_response(connectSocketFD, 404);
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
  /*
   * TODO: PART 3 is to serve both files and directories. You will need to
   * determine when to call serve_file() or serve_directory() depending
   * on `path`. Make your edits below here in this function.
   */
  close(connectSocketFD);
  return;
}

/*
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
 */
void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of SERVER_PROXY_HOSTNAME and 
  * opens a connection to it. Please do not modify.
  */
  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(SERVER_PROXY_PORT);

  // Use DNS to resolve the proxy target's IP address
  struct hostent *target_dns_entry = gethostbyname2(SERVER_PROXY_HOSTNAME, AF_INET);

  // Create an IPv4 TCP socket to communicate with the proxy target.
  int target_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (target_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    close(fd);
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", SERVER_PROXY_HOSTNAME);
    close(target_fd);
    close(fd);
    exit(ENXIO);
  }

  char *dns_address = target_dns_entry->h_addr_list[0];

  // Connect to the proxy target.
  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status = connect(target_fd, (struct sockaddr*) &target_address,
      sizeof(target_address));

  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(target_fd);
    close(fd);
    return;

  }

  /* TODO: PART 4 */

}

#ifdef POOLSERVER
/* 
 * All worker threads will run this function until the server shutsdown.
 * Each thread should block until a new request has been received.
 * When the server accepts a new connection, a thread should be dispatched
 * to send a response to the client.
 */
void *handle_clients(void *void_request_handler) {
  request_handler_func requestHandler = (request_handler_func) void_request_handler;
  /* (Valgrind) Detach so thread frees its memory on completion, since we won't
   * be joining on it. */
  pthread_detach(pthread_self());

  /* TODO: PART 7 */
  return NULL;

}

/* 
 * Creates `NUN_THREADS` amount of threads. Initializes the work queue.
 */
void init_thread_pool(int numThreads, request_handler_func requestHandler) {

  /* TODO: PART 7 */

}
#endif

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
   * The thread pool is initialized *before* the server
   * begins accepting client connections.
   */
  init_thread_pool(NUM_THREADS, REQUEST_HANDLER);
#endif

  struct sockaddr_in clientAddress;
  size_t clientAddressLen = sizeof(clientAddress);
  int connectSocketFD;

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
     * This is a single-process, single-threaded HTTP server.
     * When a client connection has been accepted, the main
     * process sends a response to the client. During this
     * time, the server does not listen or accept connections.
     * Only after a response has been sent to the client can
     * the server accept a new connection.
     */
    requestHandler(connectSocketFD);
#elif FORKSERVER
    /* 
     * TODO: PART 5 BEGIN
     *
     * When a client connection has been accepted, a new
     * process is spawned. This child process will send
     * a response to the client. Afterwards, the child
     * process should exit. During this time, the parent
     * process should continue listening and accepting
     * connections.
     */

    /* PART 5 END */
#elif THREADSERVER
    /* 
     * TODO: PART 6 BEGIN
     *
     * When a client connection has been accepted, a new
     * thread is created. This thread will send a response
     * to the client. The main thread should continue
     * listening and accepting connections. The main
     * thread will NOT be joining with the new thread.
     */

    /* PART 6 END */
#elif POOLSERVER
    /* 
     * TODO: PART 7 BEGIN
     *
     * When a client connection has been accepted, add the
     * client's socket number to the work queue. A thread
     * in the thread pool will send a response to the client.
     */

    /* PART 7 END */
#endif
  }

  shutdown(*serverSocketFD, SHUT_RDWR);
  close(*serverSocketFD);
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
