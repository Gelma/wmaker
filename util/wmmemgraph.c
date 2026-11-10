/*
 *  Window Maker memory & swap monitor dockapp
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
#define GRAPH_HEIGHT 30
#define GRAPH_X      5
#define GRAPH_Y      16

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

static int read_meminfo(unsigned long long *mem_total, unsigned long long *mem_avail,
			unsigned long long *swap_total, unsigned long long *swap_free)
{
	FILE *fp;
	char line[256];

	*mem_total = 0;
	*mem_avail = 0;
	*swap_total = 0;
	*swap_free = 0;

	fp = fopen("/proc/meminfo", "r");
	if (!fp)
		return -1;

	while (fgets(line, sizeof(line), fp)) {
		if (strncmp(line, "MemTotal:", 9) == 0) {
			if (sscanf(line + 9, "%llu", mem_total) != 1)
				*mem_total = 0;
		} else if (strncmp(line, "MemAvailable:", 13) == 0) {
			if (sscanf(line + 13, "%llu", mem_avail) != 1)
				*mem_avail = 0;
		} else if (strncmp(line, "SwapTotal:", 10) == 0) {
			if (sscanf(line + 10, "%llu", swap_total) != 1)
				*swap_total = 0;
		} else if (strncmp(line, "SwapFree:", 9) == 0) {
			if (sscanf(line + 9, "%llu", swap_free) != 1)
				*swap_free = 0;
		}
	}
	fclose(fp);
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
	unsigned long col_mem_fill, col_swap_bar, col_swap_fill, col_warn, col_crit;
	int history[GRAPH_WIDTH];
	int delay_ms = 1000;
	const char *display_name = NULL;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-display") == 0 && i + 1 < argc)
			display_name = argv[++i];
		else if (strcmp(argv[i], "-delay") == 0 && i + 1 < argc)
			delay_ms = atoi(argv[++i]);
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("Usage: wmmemgraph [-display <display>] [-delay <ms>]\n");
			return 0;
		}
	}

	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);

	dpy = XOpenDisplay(display_name);
	if (!dpy) {
		fprintf(stderr, "wmmemgraph: unable to open display %s\n", XDisplayName(display_name));
		return 1;
	}

	screen = DefaultScreen(dpy);
	cmap = DefaultColormap(dpy, screen);

	col_bg = alloc_color(dpy, cmap, "#2c2c2c", "gray20");
	col_bevel_hi = alloc_color(dpy, cmap, "#545454", "gray40");
	col_bevel_lo = alloc_color(dpy, cmap, "#141414", "black");
	col_graph_bg = alloc_color(dpy, cmap, "#10161a", "black");
	col_text = alloc_color(dpy, cmap, "#ffffff", "white");
	col_mem_fill = alloc_color(dpy, cmap, "#4499ee", "blue");
	col_swap_bar = alloc_color(dpy, cmap, "#202428", "gray15");
	col_swap_fill = alloc_color(dpy, cmap, "#33cc66", "green");
	col_warn = alloc_color(dpy, cmap, "#eeaa22", "yellow");
	col_crit = alloc_color(dpy, cmap, "#ee3333", "red");

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
		class_hint->res_name = "wlmemgraph";
		class_hint->res_class = "WLDockApp";
		XSetClassHint(dpy, win, class_hint);
		XFree(class_hint);
	}

	XSelectInput(dpy, win, ExposureMask | StructureNotifyMask);
	XSelectInput(dpy, icon_win, ExposureMask | StructureNotifyMask);

	XMapWindow(dpy, win);

	memset(history, 0, sizeof(history));

	while (running) {
		unsigned long long mem_total = 0, mem_avail = 0;
		unsigned long long swap_total = 0, swap_free = 0;
		int mem_pct = 0, swap_pct = 0;
		char text_buf[32];

		if (read_meminfo(&mem_total, &mem_avail, &swap_total, &swap_free) == 0 && mem_total > 0) {
			unsigned long long used = mem_total - mem_avail;
			mem_pct = (int)((used * 100ULL) / mem_total);
			if (swap_total > 0) {
				unsigned long long s_used = swap_total - swap_free;
				swap_pct = (int)((s_used * 100ULL) / swap_total);
			}
		}

		/* Shift graph history */
		for (i = 0; i < GRAPH_WIDTH - 1; i++)
			history[i] = history[i + 1];
		history[GRAPH_WIDTH - 1] = mem_pct;

		/* Tile background */
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

		/* Draw history bars */
		for (i = 0; i < GRAPH_WIDTH; i++) {
			int val = history[i];
			int h = (val * GRAPH_HEIGHT) / 100;
			unsigned long fill_col = col_mem_fill;

			if (val > 85)
				fill_col = col_crit;
			else if (val > 70)
				fill_col = col_warn;

			if (h > 0) {
				XSetForeground(dpy, gc, fill_col);
				XFillRectangle(dpy, pixmap, gc, GRAPH_X + i, GRAPH_Y + GRAPH_HEIGHT - h, 1, h);
			}
		}

		/* Header text */
		snprintf(text_buf, sizeof(text_buf), "MEM: %3d%%", mem_pct);
		XSetForeground(dpy, gc, col_text);
		XDrawString(dpy, pixmap, gc, 6, 12, text_buf, (int)strlen(text_buf));

		/* Bottom Swap progress bar */
		int bar_x = 5;
		int bar_y = 51;
		int bar_w = 54;
		int bar_h = 7;
		XSetForeground(dpy, gc, col_swap_bar);
		XFillRectangle(dpy, pixmap, gc, bar_x, bar_y, bar_w, bar_h);
		int fill_w = (swap_pct * bar_w) / 100;
		if (fill_w > 0) {
			unsigned long scol = col_swap_fill;
			if (swap_pct > 75)
				scol = col_crit;
			else if (swap_pct > 40)
				scol = col_warn;
			XSetForeground(dpy, gc, scol);
			XFillRectangle(dpy, pixmap, gc, bar_x, bar_y, fill_w, bar_h);
		}
		/* Swap border */
		XSetForeground(dpy, gc, col_bevel_lo);
		XDrawRectangle(dpy, pixmap, gc, bar_x, bar_y, bar_w, bar_h);

		snprintf(text_buf, sizeof(text_buf), "SWP: %2d%%", swap_pct);
		XSetForeground(dpy, gc, col_text);
		XDrawString(dpy, pixmap, gc, 8, 62, text_buf, (int)strlen(text_buf));

		/* Copy to windows */
		XCopyArea(dpy, pixmap, win, gc, 0, 0, DOCKAPP_SIZE, DOCKAPP_SIZE, 0, 0);
		XCopyArea(dpy, pixmap, icon_win, gc, 0, 0, DOCKAPP_SIZE, DOCKAPP_SIZE, 0, 0);
		XFlush(dpy);

		/* Handle events */
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

	XFreeGC(dpy, gc);
	XFreePixmap(dpy, pixmap);
	XDestroyWindow(dpy, icon_win);
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);

	return 0;
}
