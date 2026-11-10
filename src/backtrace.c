/*
 *  Window Maker window manager
 *
 *  Crash backtrace handler
 *  Idea/code inspired by https://github.com/phkaeser/wlmaker
 *
 *  Copyright (c) 2026 Window Maker Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include "backtrace.h"

#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#ifdef HAVE_LIBBACKTRACE
#include <backtrace.h>
static struct backtrace_state *w_bt_state;
#endif

static const char *exec_file_path;

static void safe_write_str(int fd, const char *str)
{
	ssize_t written;

	if (!str)
		return;
	written = write(fd, str, strlen(str));
	(void)written;
}

static void safe_write_int(int fd, int val)
{
	char buf[32];
	char *p = &buf[sizeof(buf) - 1];
	int neg = (val < 0);
	unsigned int uval = (unsigned int)(neg ? -val : val);
	ssize_t written;

	*p = '\0';
	do {
		*--p = '0' + (uval % 10);
		uval /= 10;
	} while (uval > 0 && p > buf);

	if (neg && p > buf)
		*--p = '-';

	written = write(fd, p, (size_t)(&buf[sizeof(buf) - 1] - p));
	(void)written;
}

void wBacktraceDump(int fd)
{
#if defined(HAVE_BACKTRACE) && defined(HAVE_BACKTRACE_SYMBOLS_FD)
	void *array[64];
	int size;

	safe_write_str(fd, "--- Crash Backtrace Call Stack ---\n");
	size = backtrace(array, 64);
	if (size > 0)
		backtrace_symbols_fd(array, size, fd);
	safe_write_str(fd, "--- End of Backtrace ---\n");
#elif defined(HAVE_EXECINFO_H)
	void *array[64];
	int size;

	safe_write_str(fd, "--- Crash Backtrace Call Stack ---\n");
	size = backtrace(array, 64);
	if (size > 0)
		backtrace_symbols_fd(array, size, fd);
	safe_write_str(fd, "--- End of Backtrace ---\n");
#else
	safe_write_str(fd, "--- Backtrace unavailable on this platform ---\n");
#endif
}

static void crash_signal_handler(int sig)
{
	const char *signame = "UNKNOWN";

	switch (sig) {
	case SIGSEGV:
		signame = "SIGSEGV (Segmentation fault)";
		break;
	case SIGBUS:
		signame = "SIGBUS (Bus error)";
		break;
	case SIGFPE:
		signame = "SIGFPE (Floating point exception)";
		break;
	case SIGILL:
		signame = "SIGILL (Illegal instruction)";
		break;
	case SIGABRT:
		signame = "SIGABRT (Aborted)";
		break;
	default:
		break;
	}

	safe_write_str(STDERR_FILENO, "\n=======================================================\n");
	safe_write_str(STDERR_FILENO, "Window Maker fatal error: Caught signal ");
	safe_write_int(STDERR_FILENO, sig);
	safe_write_str(STDERR_FILENO, " - ");
	safe_write_str(STDERR_FILENO, signame);
	safe_write_str(STDERR_FILENO, "\n");
	if (exec_file_path) {
		safe_write_str(STDERR_FILENO, "Executable: ");
		safe_write_str(STDERR_FILENO, exec_file_path);
		safe_write_str(STDERR_FILENO, "\n");
	}
	safe_write_str(STDERR_FILENO, "=======================================================\n");

	wBacktraceDump(STDERR_FILENO);

	/* Restore default signal handler and re-raise */
	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}

Bool wBacktraceInit(const char *exec_path)
{
	struct sigaction sa;

	exec_file_path = exec_path;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = crash_signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);
	sigaction(SIGFPE, &sa, NULL);
	sigaction(SIGILL, &sa, NULL);
	sigaction(SIGABRT, &sa, NULL);

	return True;
}
