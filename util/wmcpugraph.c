/*
 *  Window Maker multi-core CPU monitor dockapp
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
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define DOCKAPP_SIZE 64
#define GRAPH_WIDTH  54
#define GRAPH_HEIGHT 34
#define GRAPH_X      5
#define GRAPH_Y      18

typedef struct {
	unsigned long long total;
	unsigned long long idle;
} CPUTime;

static volatile sig_atomic_t running = 1;

static void sig_handler(int sig)
{
	(void)sig;
	running = 0;
}

static unsigned long alloc_color(Display *dpy, Colormap cmap, const char *name, const char *fallback)
{
	XColor color, exact;

	if (XAllocNamedColor(dpy, cmap, name, &color, &exact))
		return color.pixel;
	if (fallback && XAllocNamedColor(dpy, cmap, fallback, &color, &exact))
		return color.pixel;
	return BlackPixel(dpy, DefaultScreen(dpy));
}

static int read_cpu_times(CPUTime *agg, CPUTime **cores_out, int *num_cores_out)
{
	FILE *fp;
	char line[512];
	int core_count = 0;
	int core_cap = 16;
	CPUTime *cores = NULL;

	fp = fopen("/proc/stat", "r");
	if (!fp)
		return -1;

	cores = malloc(sizeof(CPUTime) * core_cap);
	if (!cores) {
		fclose(fp);
		return -1;
	}

	while (fgets(line, sizeof(line), fp)) {
		if (strncmp(line, "cpu ", 4) == 0) {
			unsigned long long u, n, s, i, io, ir, so, st;
			if (sscanf(line + 4, "%llu %llu %llu %llu %llu %llu %llu %llu",
				   &u, &n, &s, &i, &io, &ir, &so, &st) >= 4) {
				agg->total = u + n + s + i + io + ir + so + st;
				agg->idle = i + io;
			}
		} else if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9') {
			unsigned long long u, n, s, i, io, ir, so, st;
			if (core_count >= core_cap) {
				core_cap *= 2;
				CPUTime *tmp = realloc(cores, sizeof(CPUTime) * core_cap);
				if (!tmp)
					break;
				cores = tmp;
			}
			if (sscanf(line, "cpu%*d %llu %llu %llu %llu %llu %llu %llu %llu",
				   &u, &n, &s, &i, &io, &ir, &so, &st) >= 4) {
				cores[core_count].total = u + n + s + i + io + ir + so + st;
				cores[core_count].idle = i + io;
				core_count++;
			}
		}
	}
	fclose(fp);

	*cores_out = cores;
	*num_cores_out = core_count;
	return 0;
}

int main(int argc, char **argv)
{
	Display *dpy;
	int screen;
	Window win, icon_win;
	Pixmap pixmap;
	GC gc;
	Colormap cmap;
	XWMHints *wm_hints;
	XClassHint *class_hint;
	XSizeHints size_hints;
	unsigned long col_bg, col_bevel_hi, col_bevel_lo, col_graph_bg, col_text;
	unsigned long col_blue, col_cyan, col_green, col_yellow, col_red;
	int history[GRAPH_WIDTH];
	int history_active_cores[GRAPH_WIDTH];
	int hist_idx = 0;
	CPUTime prev_agg = { 0, 0 };
	CPUTime *prev_cores = NULL;
	int prev_num_cores = 0;
	int delay_ms = 1000;
	const char *display_name = NULL;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-display") == 0 && i + 1 < argc)
			display_name = argv[++i];
		else if (strcmp(argv[i], "-delay") == 0 && i + 1 < argc)
			delay_ms = atoi(argv[++i]);
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("Usage: wmcpugraph [-display <display>] [-delay <ms>]\n");
			return 0;
		}
	}

	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);

	dpy = XOpenDisplay(display_name);
	if (!dpy) {
		fprintf(stderr, "wmcpugraph: unable to open display %s\n", XDisplayName(display_name));
		return 1;
	}

	screen = DefaultScreen(dpy);
	cmap = DefaultColormap(dpy, screen);

	col_bg = alloc_color(dpy, cmap, "#2c2c2c", "gray20");
	col_bevel_hi = alloc_color(dpy, cmap, "#545454", "gray40");
	col_bevel_lo = alloc_color(dpy, cmap, "#141414", "black");
	col_graph_bg = alloc_color(dpy, cmap, "#10161a", "black");
	col_text = alloc_color(dpy, cmap, "#ffffff", "white");

	col_blue = alloc_color(dpy, cmap, "#3377dd", "blue");
	col_cyan = alloc_color(dpy, cmap, "#33cccc", "cyan");
	col_green = alloc_color(dpy, cmap, "#33cc44", "green");
	col_yellow = alloc_color(dpy, cmap, "#ddcc33", "yellow");
	col_red = alloc_color(dpy, cmap, "#ee3333", "red");

	win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0,
				  DOCKAPP_SIZE, DOCKAPP_SIZE, 0, col_bg, col_bg);
	icon_win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0,
				       DOCKAPP_SIZE, DOCKAPP_SIZE, 0, col_bg, col_bg);
	pixmap = XCreatePixmap(dpy, win, DOCKAPP_SIZE, DOCKAPP_SIZE, DefaultDepth(dpy, screen));
	gc = XCreateGC(dpy, pixmap, 0, NULL);

	size_hints.flags = USSize | USPosition;
	size_hints.width = DOCKAPP_SIZE;
	size_hints.height = DOCKAPP_SIZE;
	XSetWMNormalHints(dpy, win, &size_hints);

	wm_hints = XAllocWMHints();
	if (wm_hints) {
		wm_hints->flags = StateHint | IconWindowHint | WindowGroupHint;
		wm_hints->initial_state = WithdrawnState;
		wm_hints->icon_window = icon_win;
		wm_hints->window_group = win;
		XSetWMHints(dpy, win, wm_hints);
		XFree(wm_hints);
	}

	class_hint = XAllocClassHint();
	if (class_hint) {
		class_hint->res_name = "wlcpugraph";
		class_hint->res_class = "WLDockApp";
		XSetClassHint(dpy, win, class_hint);
		XFree(class_hint);
	}

	XSelectInput(dpy, win, ExposureMask | StructureNotifyMask);
	XSelectInput(dpy, icon_win, ExposureMask | StructureNotifyMask);

	XMapWindow(dpy, win);

	memset(history, 0, sizeof(history));
	memset(history_active_cores, 0, sizeof(history_active_cores));

	read_cpu_times(&prev_agg, &prev_cores, &prev_num_cores);

	while (running) {
		CPUTime curr_agg = { 0, 0 };
		CPUTime *curr_cores = NULL;
		int curr_num_cores = 0;
		int cpu_pct = 0;
		int active_cores = 0;
		char text_buf[32];

		if (read_cpu_times(&curr_agg, &curr_cores, &curr_num_cores) == 0) {
			unsigned long long total_d = curr_agg.total - prev_agg.total;
			unsigned long long idle_d = curr_agg.idle - prev_agg.idle;

			if (total_d > 0)
				cpu_pct = (int)((total_d - idle_d) * 100ULL / total_d);
			if (cpu_pct > 100)
				cpu_pct = 100;

			if (curr_num_cores > 0 && prev_num_cores == curr_num_cores) {
				int c;
				for (c = 0; c < curr_num_cores; c++) {
					unsigned long long c_tot = curr_cores[c].total - prev_cores[c].total;
					unsigned long long c_idl = curr_cores[c].idle - prev_cores[c].idle;
					if (c_tot > 0 && ((c_tot - c_idl) * 100ULL / c_tot) > 10ULL)
						active_cores++;
				}
			}
			prev_agg = curr_agg;
			free(prev_cores);
			prev_cores = curr_cores;
			prev_num_cores = curr_num_cores;
		}

		/* Shift graph history */
		for (i = 0; i < GRAPH_WIDTH - 1; i++) {
			history[i] = history[i + 1];
			history_active_cores[i] = history_active_cores[i + 1];
		}
		history[GRAPH_WIDTH - 1] = cpu_pct;
		history_active_cores[GRAPH_WIDTH - 1] = active_cores;
		hist_idx = (hist_idx + 1) % GRAPH_WIDTH;

		/* Draw tile background */
		XSetForeground(dpy, gc, col_bg);
		XFillRectangle(dpy, pixmap, gc, 0, 0, DOCKAPP_SIZE, DOCKAPP_SIZE);

		/* NeXTSTEP outer bevel */
		XSetForeground(dpy, gc, col_bevel_hi);
		XDrawLine(dpy, pixmap, gc, 0, 0, DOCKAPP_SIZE - 1, 0);
		XDrawLine(dpy, pixmap, gc, 0, 0, 0, DOCKAPP_SIZE - 1);
		XSetForeground(dpy, gc, col_bevel_lo);
		XDrawLine(dpy, pixmap, gc, 0, DOCKAPP_SIZE - 1, DOCKAPP_SIZE - 1, DOCKAPP_SIZE - 1);
		XDrawLine(dpy, pixmap, gc, DOCKAPP_SIZE - 1, 0, DOCKAPP_SIZE - 1, DOCKAPP_SIZE - 1);

		/* Inner frame for graph */
		XSetForeground(dpy, gc, col_bevel_lo);
		XDrawLine(dpy, pixmap, gc, GRAPH_X - 1, GRAPH_Y - 1, GRAPH_X + GRAPH_WIDTH, GRAPH_Y - 1);
		XDrawLine(dpy, pixmap, gc, GRAPH_X - 1, GRAPH_Y - 1, GRAPH_X - 1, GRAPH_Y + GRAPH_HEIGHT);
		XSetForeground(dpy, gc, col_bevel_hi);
		XDrawLine(dpy, pixmap, gc, GRAPH_X - 1, GRAPH_Y + GRAPH_HEIGHT, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT);
		XDrawLine(dpy, pixmap, gc, GRAPH_X + GRAPH_WIDTH, GRAPH_Y - 1, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT);

		/* Graph background */
		XSetForeground(dpy, gc, col_graph_bg);
		XFillRectangle(dpy, pixmap, gc, GRAPH_X, GRAPH_Y, GRAPH_WIDTH, GRAPH_HEIGHT);

		/* Draw columns */
		for (i = 0; i < GRAPH_WIDTH; i++) {
			int val = history[i];
			int cores_act = history_active_cores[i];
			int h = (val * GRAPH_HEIGHT) / 100;
			unsigned long col;

			if (h <= 0)
				continue;

			if (prev_num_cores > 0) {
				float ratio = (float)cores_act / (float)prev_num_cores;
				if (ratio <= 0.25f)
					col = col_blue;
				else if (ratio <= 0.50f)
					col = col_cyan;
				else if (ratio <= 0.75f)
					col = col_green;
				else if (ratio < 1.0f)
					col = col_yellow;
				else
					col = col_red;
			} else {
				col = col_green;
			}

			XSetForeground(dpy, gc, col);
			XFillRectangle(dpy, pixmap, gc, GRAPH_X + i, GRAPH_Y + GRAPH_HEIGHT - h, 1, h);
		}

		/* Header text */
		snprintf(text_buf, sizeof(text_buf), "CPU: %3d%%", cpu_pct);
		XSetForeground(dpy, gc, col_text);
		XDrawString(dpy, pixmap, gc, 6, 13, text_buf, (int)strlen(text_buf));

		/* Bottom status */
		if (prev_num_cores > 0)
			snprintf(text_buf, sizeof(text_buf), "%d CORES", prev_num_cores);
		else
			snprintf(text_buf, sizeof(text_buf), "ONLINE");
		XDrawString(dpy, pixmap, gc, 10, DOCKAPP_SIZE - 4, text_buf, (int)strlen(text_buf));

		/* Copy to windows */
		XCopyArea(dpy, pixmap, win, gc, 0, 0, DOCKAPP_SIZE, DOCKAPP_SIZE, 0, 0);
		XCopyArea(dpy, pixmap, icon_win, gc, 0, 0, DOCKAPP_SIZE, DOCKAPP_SIZE, 0, 0);
		XFlush(dpy);

		/* Wait for event or delay */
		while (XPending(dpy)) {
			XEvent ev;
			XNextEvent(dpy, &ev);
			if (ev.type == Expose && ev.xexpose.count == 0) {
				XCopyArea(dpy, pixmap, win, gc, 0, 0, DOCKAPP_SIZE, DOCKAPP_SIZE, 0, 0);
				XCopyArea(dpy, pixmap, icon_win, gc, 0, 0, DOCKAPP_SIZE, DOCKAPP_SIZE, 0, 0);
				XFlush(dpy);
			}
		}

		struct timeval tv;
		tv.tv_sec = delay_ms / 1000;
		tv.tv_usec = (delay_ms % 1000) * 1000;
		int x11_fd = ConnectionNumber(dpy);
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(x11_fd, &fds);
		select(x11_fd + 1, &fds, NULL, NULL, &tv);
	}

	free(prev_cores);
	XFreeGC(dpy, gc);
	XFreePixmap(dpy, pixmap);
	XDestroyWindow(dpy, icon_win);
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);

	return 0;
}
