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
#include <unistd.h>

#include "server_config.h"
#include "server_signal.h"
#include "libhttp.h"
#include "wq.h"

/*
 * Serves the contents the file stored at `path` to the client socket `fd`.
 * It is the caller's reponsibility to ensure that the file stored at `path` exists.
 */
void serve_file(int fd, char *path) {

  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(path));
  http_send_header(fd, "Content-Length", "0"); // Change this too
  http_end_headers(fd);

  /* TODO: PART 2 */

}

void serve_directory(int fd, char *path) {
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
  http_end_headers(fd);

  /* TODO: PART 3 */

}


/*
 * Reads an HTTP request from client socket (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 *
 *   Closes the client socket (fd) when finished.
 */
void handle_files_request(int fd) {

  struct http_request *request = http_request_parse(fd);

  if (request == NULL || request->path[0] != '/') {
    http_start_response(fd, 400);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return;
  }

  if (strstr(request->path, "..") != NULL) {
    http_start_response(fd, 403);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return;
  }

  /* Remove beginning `./` */
  char *path = malloc(2 + strlen(request->path) + 1);
  path[0] = '.';
  path[1] = '/';
  memcpy(path + 2, request->path, strlen(request->path) + 1);

  /*
   * TODO: PART 2 is to serve files. If the file given by `path` exists,
   * call serve_file() on it. Else, serve a 404 Not Found error below.
   * The `stat()` syscall will be useful here.
   *
   * TODO: PART 3 is to serve both files and directories. You will need to
   * determine when to call serve_file() or serve_directory() depending
   * on `path`. Make your edits below here in this function.
   */

  close(fd);
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
 * NOTE: there is no internal restriction on what directories can be
 * accessed, but the server should only serve files within the
 * "/home/vagrant/code/personal/hw4" directory, so just always pass
 * the correct directory in the command line.
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
    connectSocketFD = accept( *serverSocketFD, (struct sockaddr *) &clientAddress, (socklen_t *) &clientAddressLen);
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
     * time, the server does not listen and accept connections.
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

  // if the server is run in file mode, chnge the process's working directory
  if (REQUEST_HANDLER == handle_files_request) {
    change_working_directory();
  }

  serve_forever(&SERVER_SOCKET, REQUEST_HANDLER);

  return EXIT_SUCCESS;
}
