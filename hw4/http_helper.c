/**
 * This module contains helper functions for data transfer under the HTTP protocol.
 *
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "http_helper.h"

/**
 * Exit with a fatal error.
 * 
 * @param message The error message
 */
void http_fatal_error(char *message) {
  fprintf(stderr, "%s\n", message);
  exit(ENOBUFS);
}

/**
 * Send an HTTP request line.
 * 
 * @param fd The connection socket file descriptor
 * @param method The HTTP request method
 * @param path The HTTP requested path
 */
void http_start_request(int fd, const char *method, const char *path) {
  dprintf(fd, "%s %s HTTP/1.0\r\n", method, path);
}

/**
 * Parse the HTTP request line.
 * 
 * @param fd The connection socket file descriptor
 */
struct http_request *http_request_parse(int fd) {
  struct http_request *request = malloc(sizeof(struct http_request));
  if (!request) http_fatal_error("Malloc failed");

  char *read_buffer = malloc(LIBHTTP_REQUEST_MAX_SIZE + 1);
  if (!read_buffer) http_fatal_error("Malloc failed");

  int bytes_read = read(fd, read_buffer, LIBHTTP_REQUEST_MAX_SIZE);
  read_buffer[bytes_read] = '\0'; /* Always null-terminate. */

  char *read_start, *read_end;
  size_t read_size;

  do {
    /* Read in the HTTP method: "[A-Z]*" */
    read_start = read_end = read_buffer;
    while (*read_end >= 'A' && *read_end <= 'Z') read_end++;
    read_size = read_end - read_start;
    if (read_size == 0) break;
    request->method = malloc(read_size + 1);
    memcpy(request->method, read_start, read_size);
    request->method[read_size] = '\0';

    /* Read in a space character. */
    read_start = read_end;
    if (*read_end != ' ') break;
    read_end++;

    /* Read in the path: "[^ \n]*" */
    read_start = read_end;
    while (*read_end != '\0' && *read_end != ' ' && *read_end != '\n') read_end++;
    read_size = read_end - read_start;
    if (read_size == 0) break;
    request->path = malloc(read_size + 1);
    memcpy(request->path, read_start, read_size);
    request->path[read_size] = '\0';

    /* Read in HTTP version and rest of request line: ".*" */
    read_start = read_end;
    while (*read_end != '\0' && *read_end != '\n') read_end++;
    if (*read_end != '\n') break;
    read_end++;

    free(read_buffer);
    return request;
  } while (0);

  /* An error occurred. */
  free(request);
  free(read_buffer);
  return NULL;

}

/**
 * Returns the standard message corresponding to an HTTP status code.
 * 
 * @param status_code The HTTP response status code
 */
char* http_get_response_message(int status_code) {
  switch (status_code) {
    case 100:
      return "Continue";
    case 200:
      return "OK";
    case 301:
      return "Moved Permanently";
    case 302:
      return "Found";
    case 304:
      return "Not Modified";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 502:
      return "Bad Gateway";
    default:
      return "Internal Server Error";
  }
}

/**
 * Send an HTTP response line.
 * 
 * @param fd The connection socket file descriptor
 * @param status_code The response status code
 */
void http_start_response(int fd, int status_code) {
  dprintf(fd, "HTTP/1.0 %d %s\r\n", status_code,
      http_get_response_message(status_code));
}

/**
 * Send an HTTP response header line.
 * 
 * @param fd The connection socket file descriptor
 * @param key The header name
 * @param value The header value
 */
void http_send_header(int fd, char *key, char *value) {
  dprintf(fd, "%s: %s\r\n", key, value);
}

/**
 * End the HTTP response header section.
 * 
 * @param fd The connection socket file descriptor
 */
void http_end_headers(int fd) {
  dprintf(fd, "\r\n");
}

/**
 * Create a hyperlink from a file path.
 * 
 * Puts `<a href="/path/filename">filename</a><br/>` into the provided buffer.
 * The resulting string in the buffer is null-terminated. It is the caller's
 * responsibility to ensure that the buffer has enough space for the resulting string.
 * 
 * @param buffer The string buffer used to store the hyperlink output
 * @param path The path of the directory
 * @param filename The name of the file
 */
void http_format_href(char *buffer, char *path, char *filename) {
  int length = strlen("<a href=\"//\"></a><br/>") + strlen(path) + strlen(filename)*2 + 1;
  snprintf(buffer, length, "<a href=\"/%s/%s\">%s</a><br/>", path, filename, filename);
}

/**
 * Create the file path of an index file.
 * 
 * Puts `path/index.html` into the provided buffer.
 * The resulting string in the buffer is null-terminated.
 * It is the caller's responsibility to ensure that the
 * buffer has enough space for the resulting string.
 * 
 * @param buffer The string buffer used to store the file path output
 * @param path The path of the directory
 */
void http_format_index(char *buffer, char *path) {
  int length = strlen(path) + strlen("/index.html") + 1;
  snprintf(buffer, length, "%s/index.html", path);
}

/**
 * Returns the HTTP Content-Type based on a file name.
 * 
 * @param file_name The name of the file
 */
char *http_get_mime_type(char *file_name) {
  char *file_extension = strrchr(file_name, '.');
  if (file_extension == NULL) {
    return "text/plain";
  }

  if (strcmp(file_extension, ".html") == 0 || strcmp(file_extension, ".htm") == 0) {
    return "text/html";
  } else if (strcmp(file_extension, ".jpg") == 0 || strcmp(file_extension, ".jpeg") == 0) {
    return "image/jpeg";
  } else if (strcmp(file_extension, ".png") == 0) {
    return "image/png";
  } else if (strcmp(file_extension, ".css") == 0) {
    return "text/css";
  } else if (strcmp(file_extension, ".js") == 0) {
    return "application/javascript";
  } else if (strcmp(file_extension, ".pdf") == 0) {
    return "application/pdf";
  } else {
    return "text/plain";
  }
}
