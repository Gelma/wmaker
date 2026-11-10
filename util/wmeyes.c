/*
 *  Window Maker mouse-tracking eyes dockapp
 *  Idea/code inspired by https://github.com/phkaeser/wlmaker
 *
 *  Copyright (c) 2026 Window Maker Team
 *  Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
 *  Copyright 2025 Google LLC
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
#include <math.h>
#include <time.h>
#include <sys/select.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define DOCKAPP_SIZE 64

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

/* Eye drawing helper based on wlmeyes.c geometry */
static void draw_eye(Display *dpy, Drawable d, GC gc, unsigned long white,
		     unsigned long border, unsigned long pupil_color,
		     int cx, int cy, int ew, int eh,
		     int ptr_x, int ptr_y)
{
	int x = cx - ew / 2;
	int y = cy - eh / 2;

	/* Draw sclera (white) */
	XSetForeground(dpy, gc, white);
	XFillArc(dpy, d, gc, x, y, ew, eh, 0, 360 * 64);

	/* Draw border */
	XSetForeground(dpy, gc, border);
	XDrawArc(dpy, d, gc, x, y, ew, eh, 0, 360 * 64);

	/* Calculate pupil offset */
	double rel_x = (double)(ptr_x - cx);
	double rel_y = (double)(ptr_y - cy);
	double max_rx = (double)(ew / 2 - 5);
	double max_ry = (double)(eh / 2 - 5);

	if (max_rx < 1.0) max_rx = 1.0;
	if (max_ry < 1.0) max_ry = 1.0;

	double ratio = (rel_x * rel_x) / (max_rx * max_rx) + (rel_y * rel_y) / (max_ry * max_ry);
	if (ratio > 1.0) {
		double scale = 1.0 / sqrt(ratio);
		rel_x *= scale;
		rel_y *= scale;
	}

	int px = cx + (int)round(rel_x);
	int py = cy + (int)round(rel_y);
	int pr = 4;

	/* Draw pupil */
	XSetForeground(dpy, gc, pupil_color);
	XFillArc(dpy, d, gc, px - pr, py - pr, pr * 2, pr * 2, 0, 360 * 64);
}

int main(int argc, char **argv)
{
	Display *dpy;
	int screen;
	Window root, win;
	Pixmap pixmap;
	GC gc;
	Colormap cmap;
	XWMHints hints;
	XSizeHints size_hints;
	XClassHint class_hint;
	struct sigaction sa;
	const char *display_name = NULL;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-display") == 0 && i + 1 < argc) {
			display_name = argv[++i];
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("Usage: %s [-display <display>]\n", argv[0]);
			printf("Window Maker mouse-tracking eyes dockapp\n");
			return EXIT_SUCCESS;
		}
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	dpy = XOpenDisplay(display_name);
	if (!dpy) {
		fprintf(stderr, "%s: cannot open display\n", argv[0]);
		return EXIT_FAILURE;
	}

	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	cmap = DefaultColormap(dpy, screen);

	unsigned long col_bg     = alloc_color(dpy, cmap, "#2c2c2c", "black");
	unsigned long col_border = alloc_color(dpy, cmap, "#101010", "black");
	unsigned long col_white  = alloc_color(dpy, cmap, "#ffffff", "white");
	unsigned long col_pupil  = alloc_color(dpy, cmap, "#000000", "black");
	unsigned long col_bevel_hi = alloc_color(dpy, cmap, "#555555", "gray");
	unsigned long col_bevel_lo = alloc_color(dpy, cmap, "#151515", "black");

	win = XCreateSimpleWindow(dpy, root, 0, 0, DOCKAPP_SIZE, DOCKAPP_SIZE,
				  0, col_border, col_bg);
	pixmap = XCreatePixmap(dpy, win, DOCKAPP_SIZE, DOCKAPP_SIZE,
			       DefaultDepth(dpy, screen));
	gc = XCreateGC(dpy, win, 0, NULL);

	memset(&hints, 0, sizeof(hints));
	hints.flags = StateHint | IconWindowHint;
	hints.initial_state = WithdrawnState;
	hints.icon_window = win;
	XSetWMHints(dpy, win, &hints);

	memset(&size_hints, 0, sizeof(size_hints));
	size_hints.flags = PMinSize | PMaxSize;
	size_hints.min_width = DOCKAPP_SIZE;
	size_hints.max_width = DOCKAPP_SIZE;
	size_hints.min_height = DOCKAPP_SIZE;
	size_hints.max_height = DOCKAPP_SIZE;
	XSetWMNormalHints(dpy, win, &size_hints);

	class_hint.res_name = (char *)"wmeyes";
	class_hint.res_class = (char *)"WMEyes";
	XSetClassHint(dpy, win, &class_hint);

	XStoreName(dpy, win, "WMEyes");
	XSelectInput(dpy, win, ExposureMask | StructureNotifyMask);
	XMapWindow(dpy, win);

	while (running) {
		while (XPending(dpy)) {
			XEvent ev;
			XNextEvent(dpy, &ev);
			if (ev.type == DestroyNotify)
				running = 0;
		}
		if (!running)
			break;

		/* Query pointer position */
		Window root_ret, child_ret;
		int root_x = 0, root_y = 0;
		int win_x = 0, win_y = 0;
		unsigned int mask;

		XQueryPointer(dpy, win, &root_ret, &child_ret,
			      &root_x, &root_y, &win_x, &win_y, &mask);

		/* Background */
		XSetForeground(dpy, gc, col_bg);
		XFillRectangle(dpy, pixmap, gc, 0, 0, DOCKAPP_SIZE, DOCKAPP_SIZE);

		/* NeXTSTEP 3D DockApp frame bevel */
		XSetForeground(dpy, gc, col_bevel_hi);
		XDrawLine(dpy, pixmap, gc, 0, 0, DOCKAPP_SIZE - 1, 0);
		XDrawLine(dpy, pixmap, gc, 0, 0, 0, DOCKAPP_SIZE - 1);
		XSetForeground(dpy, gc, col_bevel_lo);
		XDrawLine(dpy, pixmap, gc, 1, DOCKAPP_SIZE - 1, DOCKAPP_SIZE - 1, DOCKAPP_SIZE - 1);
		XDrawLine(dpy, pixmap, gc, DOCKAPP_SIZE - 1, 1, DOCKAPP_SIZE - 1, DOCKAPP_SIZE - 1);

		/* Draw eyes (cx=18, cy=32 for left; cx=46, cy=32 for right) */
		draw_eye(dpy, pixmap, gc, col_white, col_border, col_pupil,
			 18, 32, 22, 38, win_x, win_y);
		draw_eye(dpy, pixmap, gc, col_white, col_border, col_pupil,
			 46, 32, 22, 38, win_x, win_y);

		/* Copy double buffer to window */
		XCopyArea(dpy, pixmap, win, gc, 0, 0, DOCKAPP_SIZE, DOCKAPP_SIZE, 0, 0);
		XFlush(dpy);

		/* Sleep ~35ms (~28 FPS tracking) */
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 35000;
		select(0, NULL, NULL, NULL, &tv);
	}

	XFreeGC(dpy, gc);
	XFreePixmap(dpy, pixmap);
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);

	return EXIT_SUCCESS;
}
