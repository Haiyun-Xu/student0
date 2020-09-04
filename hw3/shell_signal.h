/**
 * This module contains helper functions for handling signals sent to the
 * shell process and its subprocesses.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef SHELL_SIGNAL_H
#define SHELL_SIGNAL_H

#include <stdio.h>
#include <signal.h>

/**
 * A group of signals. The maximum number of signals that can be contained in
 * the struct is3, because currently there are 32 POSIX signals.
 */
struct signal_group {
  const int numOfSignals;
  const int signals[33];
};

/**
 * Get the set of ignored signals.
 * 
 * @return const struct signal_group* The set of ignored signaals
 */
const struct signal_group *get_ignored_signals();

/**
 * Ignores a signal. This is a substitute of the built-in SIG_IGN option,
 * because the effect of the latter persists through an execve() call and
 * will result in the new process image to ignore the signal as well.
 * 
 * @param signalID The numeric identifier of the signal
 */
void ignore_signal(int signalID);

#endif /* SHELL_SIGNAL_H */