/**
 * This module contains helper functions for data transfer under the HTTP protocol.
 *
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef HTTP_HELPER_H
#define HTTP_HELPER_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Usage example:
 *
 *     struct http_request *request = http_request_parse(fd);
 *     ...
 *     http_start_response(fd, 200);
 *     http_send_header(fd, "Content-type", http_get_mime_type("index.html"));
 *     http_send_header(fd, "Server", "httpserver/1.0");
 *     http_end_headers(fd);
 *     http_send_string(fd, "<html><body><a href='/'>Home</a></body></html>");
 *     ...
 *     close(fd);
 */

#define LIBHTTP_REQUEST_MAX_SIZE 8192

/**
 * A simple HTTP request struct
 */
struct http_request {
  char *method;
  char *path;
};

/**
 * Send an HTTP request line.
 * 
 * @param fd The connection socket file descriptor
 * @param method The HTTP request method
 * @param path The HTTP requested path
 */
void http_start_request(int fd, const char *method, const char *path);

/**
 * Parse the HTTP request line.
 * 
 * @param fd The connection socket file descriptor
 */
struct http_request *http_request_parse(int fd);

/**
 * Send an HTTP response line.
 * 
 * @param fd The connection socket file descriptor
 * @param status_code The response status code
 */
void http_start_response(int fd, int status_code);

/**
 * Send an HTTP response header line.
 * 
 * @param fd The connection socket file descriptor
 * @param key The header name
 * @param value The header value
 */
void http_send_header(int fd, char *key, char *value);

/**
 * End the HTTP response header section.
 * 
 * @param fd The connection socket file descriptor
 */
void http_end_headers(int fd);

/**
 * Create a hyperlink from a file path.
 * 
 * @param buffer The string buffer used to store the hyperlink output
 * @param path The path of the directory
 * @param filename The name of the file
 */
void http_format_href(char *buffer, char *path, char *filename);

/**
 * Create the file path of an index file.
 * 
 * @param buffer The string buffer used to store the file path output
 * @param path The path of the directory
 */
void http_format_index(char *buffer, char *path);

/**
 * Returns the HTTP Content-Type based on a file name.
 * 
 * @param file_name The name of the file
 */
char *http_get_mime_type(char *file_name);

#endif /* HTTP_HELPER_H */
