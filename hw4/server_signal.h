/**
 * This module contains error handling logic for the server.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef SERVER_SIGNAL_H
#define SERVER_SIGNAL_H

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "server_config.h"

/**
 * Handles received signals.
 * 
 * @return void
 */
void handle_signals();


#endif /* SERVER_SIGNAL_H */