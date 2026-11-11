/*
 *  Window Maker window manager
 *
 *  Subprocess monitoring with non-blocking pipe capture
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

#ifndef WMSUBPROCESS_H_
#define WMSUBPROCESS_H_

#include "WindowMaker.h"
#include "window.h"
#include <sys/types.h>

typedef struct WSubprocess WSubprocess;

/* Upper bound on the captured stdout of a single subprocess: output beyond
 * this is dropped so a chatty child cannot grow the WM heap without bound. */
#define W_SUBPROCESS_MAX_CAPTURE (1024 * 1024)

/*
 * Termination callback.  exit_status is the *raw* wait status as delivered by
 * waitpid() (same convention as wAddDeathHandler()): decode it with
 * WIFEXITED()/WEXITSTATUS()/WIFSIGNALED(), do not treat it as an exit code.
 * The WSubprocess handle is freed right after the callback returns: it must
 * not be used afterwards.
 */
typedef void (*WSubprocessTerminatedCallback)(WSubprocess *proc, int exit_status, void *user_data);

struct WSubprocess {
	pid_t pid;
	char *command;
	int stdout_fd;
	int stderr_fd;
	WMHandlerID *stdout_handler;
	WMHandlerID *stderr_handler;
	WSubprocessTerminatedCallback term_cb;
	void *user_data;
	WMData *stdout_buffer;
	WMagicNumber death_handler_id;
};

/*
 * Run a subprocess with monitored non-blocking stdout/stderr capture.
 * If capture_stdout is True, stdout is collected in proc->stdout_buffer (a
 * WMData, readable with WMGetDataBytes()/WMGetDataLength()) up to
 * W_SUBPROCESS_MAX_CAPTURE bytes; stderr is logged, not captured.
 * Returns NULL if the command is empty or the pipes/fork fail.
 */
WSubprocess *wSubprocessRun(const char *command,
			    Bool capture_stdout,
			    WSubprocessTerminatedCallback term_cb,
			    void *user_data);

/*
 * Terminate and destroy a subprocess handle. If still running, sends SIGTERM.
 */
void wSubprocessKill(WSubprocess *proc);

#endif /* WMSUBPROCESS_H_ */
