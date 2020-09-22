/**
 * This module contains helper functions for handling signals sent to the
 * shell process and its subprocesses.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "shell_signal.h"

/**
 * A group of signals. The maximum number of signals that can be contained in
 * the struct is 33, because Linux currently supports 33 different real-time
 * signals
 */
struct signal_group {
  const int numOfSignals;
  const int signals[33];
};

/* 
 * SIGINT, SIGQUIT, and SIGSTP are signals that can be sent by the terminal,
 * and so they should be ignored .
 * 
 * If the shell is not in the foreground, then the terminal signals would
 * be sent to processes in the foreground, which makes the signals that
 * the shell receives (while being in the background) truly targeted at
 * the shell. If the shell is in the foreground, then it will receive the
 * terminal signals, but should ignore them.
 */
static const struct signal_group IGNORED_SIGNALS = {
  3,
  {SIGINT, SIGQUIT, SIGTSTP}
};

/**
 * Get the set of ignored signals.
 * 
 * @return const struct signal_group* The set of ignored signaals
 */
const struct signal_group *get_ignored_signals() {
  return &IGNORED_SIGNALS;
}

/**
 * A custom signal handler that ignores the signal.
 * 
 * @param sigNum The signal number
 */
void signal_ignorer(int sigNum) {
  return;
}

/**
 * Configures the current process to ignore the list of ignored signal.
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int ignore_signals() {
  const struct signal_group *signalsToIgnore = get_ignored_signals();
  int result = 0;

  /* 
   * each of the signals should be ignored, and if the signal interrupts a
   * system call, the system call should NOT be automatically restarted
   */
  struct sigaction signalAction;
  signalAction.sa_handler = signal_ignorer;
  signalAction.sa_flags = 0;

  for (int index = 0; index < signalsToIgnore->numOfSignals; index++) {
    result = sigaction(signalsToIgnore->signals[index], &signalAction, NULL);
    if (result == -1) {
      perror("Failed to register signal handler: ");
      return -1;
    }
  }
  return 0;
}

/**
 * Resets the current process's ignored signal handlers to the default hanlder.
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int reset_ignored_signals() {
  struct sigaction signalAction;
  signalAction.sa_handler = SIG_DFL;
  const struct signal_group *signalsToIgnore = get_ignored_signals();
  int result = 0;

  for (int index = 0; index < signalsToIgnore->numOfSignals; index++) {
    result = sigaction(signalsToIgnore->signals[index], &signalAction, NULL);
    if (result == -1) {
      perror("Failed to reset signal handler: ");
      return -1;
    }
  }
  return 0;
}

/**
 * Configures the current process to handle SIG_CHLD customarily.
 * 
 * @return int Returns 0 if successful, or -1 otherwise
 */
int handle_sigchld() {
  /*
   * Since a terminated background subprocess should be removed from the process
   * list by the shell process (via synchronous, non-blocking waiting), the
   * terminated subprocess must be kept in the zombie state and not reaped by
   * the SIG_CHLD handler.
   */
  struct sigaction sigAction;
  sigAction.sa_handler = signal_ignorer;
  /*
   * SA_NOCLDSTOP: do not send signal if the child process was stopped;
   * SA_RESTART: SIG_CLD shouldn't fail a syscall, so restart the interrupted syscall
   */
  sigAction.sa_flags = SA_NOCLDSTOP|SA_RESTART;

  int result = sigaction(SIGCHLD, &sigAction, NULL);
  if (result == -1) {
    perror("Failed to register signal handler: ");
    return -1;
  }
  return 0;
}

/**
 * Register signal handlers for the shell.
 * 
 * @return int Returns 0 if successful, or -1 if failed
 */
int register_shell_signal_handlers() {
  int result = 0;

  // ignore the terminal signals
  result = ignore_signals();
  if (result == -1) {
    return -1;
  }
  result = handle_sigchld();
  if (result == -1) {
    return -1;
  }
  
  // handle 
  return 0;
}