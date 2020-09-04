/**
 * This module contains helper functions for handling signals sent to the
 * shell process and its subprocesses.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "shell_signal.h"

/*
 * the set of signals that the shell process should ignore;
 * SIGKILL and SIGSTOP cannot be ignored;
 * 
 * As long as forked subprocesses are placed in the foreground, signals
 * received by the shell will truly be intended for the shell and do not
 * need to be re-routed to its foreground subprocess. Therefore, when
 * considering how the shell should handle a signal, we don't need to
 * consider rerouting the signal to another process.
 * 
 * Reasons for handling the following signals in the shell:
 * 1) SIGINT: terminates the process; should ignore this, because the shell
 * shouldn't be terminated by anything other than an "exit" command;
 * 2) SIGPIPE: if a shell receives this signal, then it's writing to a pipe
 * with no readers and it should fail; by ignore this signal, the shell
 * process would fail with an error EPIPE;
 * 3) SIGQUIT: similar to SIGINT; should ignore this;
 * 4) SIGTERM: similar to SIGQUIT, but also produce a core dump; should
 * ignore this;
 * 5) SIGTSTP: similar to SIGSTOP; should ignore this;
 */
static const struct signal_group IGNORED_SIGNALS = {
  5,
  {SIGINT, SIGPIPE, SIGQUIT, SIGTERM, SIGTSTP}
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
 * Ignores a signal. This is a substitute of the built-in SIG_IGN option,
 * because the effect of the latter persists through an execve() call and
 * will result in the new process image to ignore the signal as well.
 * 
 * @param signalID The numeric identifier of the signal
 */
void ignore_signal(int signalID) {
  return;
}