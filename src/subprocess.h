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
 * If capture_stdout is True, stdout is collected in proc->stdout_buffer.
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
