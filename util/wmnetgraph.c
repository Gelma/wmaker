/*
 *  Window Maker network bandwidth dockapp
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

static int read_net_dev(unsigned long long *rx_bytes, unsigned long long *tx_bytes)
{
	FILE *fp;
	char line[512];

	*rx_bytes = 0;
	*tx_bytes = 0;

	fp = fopen("/proc/net/dev", "r");
	if (!fp)
		return -1;

	/* Skip 2 header lines */
	if (!fgets(line, sizeof(line), fp) || !fgets(line, sizeof(line), fp)) {
		fclose(fp);
		return -1;
	}

	while (fgets(line, sizeof(line), fp)) {
		char *colon = strchr(line, ':');
		if (!colon)
			continue;

		/* Check interface name */
		char iface[32];
		if (sscanf(line, "%31s", iface) != 1)
			continue;
		if (strncmp(iface, "lo:", 3) == 0)
			continue;

		unsigned long long r_b = 0, t_b = 0;
		if (sscanf(colon + 1, "%llu %*u %*u %*u %*u %*u %*u %*u %llu", &r_b, &t_b) >= 2) {
			*rx_bytes += r_b;
			*tx_bytes += t_b;
		}
	}

	fclose(fp);
	return 0;
}

static void format_rate(unsigned long long bytes_sec, char *buf, size_t buf_sz)
{
	if (bytes_sec >= 1024ULL * 1024ULL * 1024ULL)
		snprintf(buf, buf_sz, "%.1f GB/s", (double)bytes_sec / (1024.0 * 1024.0 * 1024.0));
	else if (bytes_sec >= 1024ULL * 1024ULL)
		snprintf(buf, buf_sz, "%.1f MB/s", (double)bytes_sec / (1024.0 * 1024.0));
	else if (bytes_sec >= 1024ULL)
		snprintf(buf, buf_sz, "%llu KB/s", bytes_sec / 1024ULL);
	else
		snprintf(buf, buf_sz, "%llu B/s", bytes_sec);
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
	unsigned long col_rx, col_tx;
	int history_rx[GRAPH_WIDTH];
	int history_tx[GRAPH_WIDTH];
	unsigned long long prev_rx = 0, prev_tx = 0;
	unsigned long long peak_rate = 1024ULL * 1024ULL; /* Start scale at 1 MB/s */
	int delay_ms = 1000;
	const char *display_name = NULL;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-display") == 0 && i + 1 < argc)
			display_name = argv[++i];
		else if (strcmp(argv[i], "-delay") == 0 && i + 1 < argc)
			delay_ms = atoi(argv[++i]);
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("Usage: wmnetgraph [-display <display>] [-delay <ms>]\n");
			return 0;
		}
	}

	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);

	dpy = XOpenDisplay(display_name);
	if (!dpy) {
		fprintf(stderr, "wmnetgraph: unable to open display %s\n", XDisplayName(display_name));
		return 1;
	}

	screen = DefaultScreen(dpy);
	cmap = DefaultColormap(dpy, screen);

	col_bg = alloc_color(dpy, cmap, "#2c2c2c", "gray20");
	col_bevel_hi = alloc_color(dpy, cmap, "#545454", "gray40");
	col_bevel_lo = alloc_color(dpy, cmap, "#141414", "black");
	col_graph_bg = alloc_color(dpy, cmap, "#10161a", "black");
	col_text = alloc_color(dpy, cmap, "#ffffff", "white");
	col_rx = alloc_color(dpy, cmap, "#3388ee", "blue");
	col_tx = alloc_color(dpy, cmap, "#33ddaa", "cyan");

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
		class_hint->res_name = "wlnetgraph";
		class_hint->res_class = "WLDockApp";
		XSetClassHint(dpy, win, class_hint);
		XFree(class_hint);
	}

	XSelectInput(dpy, win, ExposureMask | StructureNotifyMask);
	XSelectInput(dpy, icon_win, ExposureMask | StructureNotifyMask);

	XMapWindow(dpy, win);

	memset(history_rx, 0, sizeof(history_rx));
	memset(history_tx, 0, sizeof(history_tx));

	read_net_dev(&prev_rx, &prev_tx);

	while (running) {
		unsigned long long curr_rx = 0, curr_tx = 0;
		unsigned long long rx_rate = 0, tx_rate = 0, total_rate = 0;
		char text_buf[32];

		if (read_net_dev(&curr_rx, &curr_tx) == 0) {
			if (curr_rx >= prev_rx)
				rx_rate = curr_rx - prev_rx;
			if (curr_tx >= prev_tx)
				tx_rate = curr_tx - prev_tx;
			total_rate = rx_rate + tx_rate;
			prev_rx = curr_rx;
			prev_tx = curr_tx;

			if (total_rate > peak_rate)
				peak_rate = total_rate;
			else if (peak_rate > 1024ULL * 1024ULL)
				peak_rate = peak_rate - (peak_rate / 128ULL);
		}

		/* Shift graph history */
		for (i = 0; i < GRAPH_WIDTH - 1; i++) {
			history_rx[i] = history_rx[i + 1];
			history_tx[i] = history_tx[i + 1];
		}
		history_rx[GRAPH_WIDTH - 1] = (int)((rx_rate * (unsigned long long)GRAPH_HEIGHT) / peak_rate);
		history_tx[GRAPH_WIDTH - 1] = (int)((tx_rate * (unsigned long long)GRAPH_HEIGHT) / peak_rate);

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
			int h_rx = history_rx[i];
			int h_tx = history_tx[i];
			if (h_rx > GRAPH_HEIGHT)
				h_rx = GRAPH_HEIGHT;
			if (h_tx > GRAPH_HEIGHT)
				h_tx = GRAPH_HEIGHT;

			if (h_rx > 0) {
				XSetForeground(dpy, gc, col_rx);
				XFillRectangle(dpy, pixmap, gc, GRAPH_X + i, GRAPH_Y + GRAPH_HEIGHT - h_rx, 1, h_rx);
			}
			if (h_tx > 0) {
				XSetForeground(dpy, gc, col_tx);
				XFillRectangle(dpy, pixmap, gc, GRAPH_X + i, GRAPH_Y, 1, h_tx);
			}
		}

		/* Header text */
		format_rate(total_rate, text_buf, sizeof(text_buf));
		char title[32];
		snprintf(title, sizeof(title), "NET: %.26s", text_buf);
		XSetForeground(dpy, gc, col_text);
		XDrawString(dpy, pixmap, gc, 6, 12, title, (int)strlen(title));

		/* Bottom status: Rx/Tx legend with peak rate */
		char peak_str[32];
		format_rate(peak_rate, peak_str, sizeof(peak_str));
		snprintf(text_buf, sizeof(text_buf), "PK:%.28s", peak_str);
		XSetForeground(dpy, gc, col_text);
		XDrawString(dpy, pixmap, gc, 6, DOCKAPP_SIZE - 5, text_buf, (int)strlen(text_buf));

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
