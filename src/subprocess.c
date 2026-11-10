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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "subprocess.h"
#include "screen.h"
#include "event.h"

static void subprocess_read_fd(WSubprocess *proc, int *fd_ptr, WMHandlerID **handler_ptr,
			       Bool is_stderr)
{
	char buf[1024];
	ssize_t n;

	if (!proc || !fd_ptr || *fd_ptr < 0)
		return;

	while ((n = read(*fd_ptr, buf, sizeof(buf) - 1)) > 0) {
		buf[n] = '\0';
		if (!is_stderr && proc->stdout_buffer) {
			WMAppendDataBytes(proc->stdout_buffer, buf, (unsigned int)n);
		} else {
			if (is_stderr)
				wwarning("[subprocess %s:%d stderr]: %s",
					 proc->command ? proc->command : "cmd",
					 (int)proc->pid, buf);
			else
				wmessage("[subprocess %s:%d stdout]: %s",
					 proc->command ? proc->command : "cmd",
					 (int)proc->pid, buf);
		}
	}

	if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
		/* Pipe closed or error */
		if (handler_ptr && *handler_ptr) {
			WMDeleteInputHandler(*handler_ptr);
			*handler_ptr = NULL;
		}
		close(*fd_ptr);
		*fd_ptr = -1;
	}
}

static void subprocess_read_stdout_cb(int fd, int mask, void *client_data)
{
	WSubprocess *proc = (WSubprocess *)client_data;

	(void)fd;
	(void)mask;
	subprocess_read_fd(proc, &proc->stdout_fd, &proc->stdout_handler, False);
}

static void subprocess_read_stderr_cb(int fd, int mask, void *client_data)
{
	WSubprocess *proc = (WSubprocess *)client_data;

	(void)fd;
	(void)mask;
	subprocess_read_fd(proc, &proc->stderr_fd, &proc->stderr_handler, True);
}

static void subprocess_death_handler(pid_t pid, unsigned int status, void *cdata)
{
	WSubprocess *proc = (WSubprocess *)cdata;

	(void)pid;
	if (!proc)
		return;

	/* Drain remaining output */
	if (proc->stdout_fd >= 0)
		subprocess_read_fd(proc, &proc->stdout_fd, &proc->stdout_handler, False);
	if (proc->stderr_fd >= 0)
		subprocess_read_fd(proc, &proc->stderr_fd, &proc->stderr_handler, True);

	if (proc->stdout_handler) {
		WMDeleteInputHandler(proc->stdout_handler);
		proc->stdout_handler = NULL;
	}
	if (proc->stderr_handler) {
		WMDeleteInputHandler(proc->stderr_handler);
		proc->stderr_handler = NULL;
	}
	if (proc->stdout_fd >= 0) {
		close(proc->stdout_fd);
		proc->stdout_fd = -1;
	}
	if (proc->stderr_fd >= 0) {
		close(proc->stderr_fd);
		proc->stderr_fd = -1;
	}

	if (proc->term_cb)
		proc->term_cb(proc, (int)status, proc->user_data);

	if (proc->command)
		wfree(proc->command);
	if (proc->stdout_buffer)
		WMReleaseData(proc->stdout_buffer);

	wfree(proc);
}

static int set_nonblocking_cloexec(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFD);
	if (flags < 0 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0)
		return -1;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		return -1;

	return 0;
}

WSubprocess *wSubprocessRun(const char *command,
			    Bool capture_stdout,
			    WSubprocessTerminatedCallback term_cb,
			    void *user_data)
{
	int stdout_pipe[2];
	int stderr_pipe[2];
	pid_t pid;
	WSubprocess *proc;

	if (!command || !*command)
		return NULL;

	if (pipe(stdout_pipe) < 0)
		return NULL;

	if (pipe(stderr_pipe) < 0) {
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		return NULL;
	}

	set_nonblocking_cloexec(stdout_pipe[0]);
	set_nonblocking_cloexec(stderr_pipe[0]);

	pid = fork();
	if (pid < 0) {
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		close(stderr_pipe[0]);
		close(stderr_pipe[1]);
		return NULL;
	}

	if (pid == 0) {
		/* Child process */
		close(stdout_pipe[0]);
		close(stderr_pipe[0]);

		if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0 ||
		    dup2(stderr_pipe[1], STDERR_FILENO) < 0) {
			_exit(127);
		}

		close(stdout_pipe[1]);
		close(stderr_pipe[1]);

		/* Reset signals to defaults in child */
		signal(SIGPIPE, SIG_DFL);
		signal(SIGCHLD, SIG_DFL);

		execl("/bin/sh", "sh", "-c", command, NULL);
		_exit(127);
	}

	/* Parent process */
	close(stdout_pipe[1]);
	close(stderr_pipe[1]);

	proc = wmalloc(sizeof(WSubprocess));
	proc->pid = pid;
	proc->command = wstrdup(command);
	proc->stdout_fd = stdout_pipe[0];
	proc->stderr_fd = stderr_pipe[0];
	proc->term_cb = term_cb;
	proc->user_data = user_data;
	proc->stdout_buffer = capture_stdout ? WMCreateDataWithCapacity(512) : NULL;

	proc->stdout_handler = WMAddInputHandler(proc->stdout_fd, WIReadMask,
						 subprocess_read_stdout_cb, proc);
	proc->stderr_handler = WMAddInputHandler(proc->stderr_fd, WIReadMask,
						 subprocess_read_stderr_cb, proc);
	proc->death_handler_id = wAddDeathHandler(proc->pid, subprocess_death_handler, proc);

	return proc;
}

void wSubprocessKill(WSubprocess *proc)
{
	if (!proc)
		return;

	if (proc->pid > 0)
		kill(proc->pid, SIGTERM);
}
