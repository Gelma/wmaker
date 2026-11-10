/*
 *  Window Maker analog & digital clock dockapp
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
#include <math.h>
#include <time.h>
#include <sys/select.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define DOCKAPP_SIZE 64
#define CLOCK_CX 32
#define CLOCK_CY 28
#define CLOCK_R  22

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
	unsigned long col_bg, col_bevel_hi, col_bevel_lo, col_text;
	unsigned long col_dial_bg, col_ticks, col_hour, col_min, col_sec;
	const char *display_name = NULL;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-display") == 0 && i + 1 < argc)
			display_name = argv[++i];
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("Usage: wmclock [-display <display>]\n");
			return 0;
		}
	}

	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);

	dpy = XOpenDisplay(display_name);
	if (!dpy) {
		fprintf(stderr, "wmclock: unable to open display %s\n", XDisplayName(display_name));
		return 1;
	}

	screen = DefaultScreen(dpy);
	cmap = DefaultColormap(dpy, screen);

	col_bg = alloc_color(dpy, cmap, "#2c2c2c", "gray20");
	col_bevel_hi = alloc_color(dpy, cmap, "#545454", "gray40");
	col_bevel_lo = alloc_color(dpy, cmap, "#141414", "black");
	col_dial_bg = alloc_color(dpy, cmap, "#1c1e22", "black");
	col_ticks = alloc_color(dpy, cmap, "#888888", "gray60");
	col_text = alloc_color(dpy, cmap, "#ffffff", "white");
	col_hour = alloc_color(dpy, cmap, "#dddddd", "white");
	col_min = alloc_color(dpy, cmap, "#44aaff", "cyan");
	col_sec = alloc_color(dpy, cmap, "#ee3333", "red");

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
		class_hint->res_name = "wmclock";
		class_hint->res_class = "DockApp";
		XSetClassHint(dpy, win, class_hint);
		XFree(class_hint);
	}

	XSelectInput(dpy, win, ExposureMask | StructureNotifyMask);
	XSelectInput(dpy, icon_win, ExposureMask | StructureNotifyMask);

	XMapWindow(dpy, win);

	while (running) {
		time_t now = time(NULL);
		struct tm *tm = localtime(&now);
		char date_buf[32];

		/* Background */
		XSetForeground(dpy, gc, col_bg);
		XFillRectangle(dpy, pixmap, gc, 0, 0, DOCKAPP_SIZE, DOCKAPP_SIZE);

		/* NeXTSTEP outer bevel */
		XSetForeground(dpy, gc, col_bevel_hi);
		XDrawLine(dpy, pixmap, gc, 0, 0, DOCKAPP_SIZE - 1, 0);
		XDrawLine(dpy, pixmap, gc, 0, 0, 0, DOCKAPP_SIZE - 1);
		XSetForeground(dpy, gc, col_bevel_lo);
		XDrawLine(dpy, pixmap, gc, 0, DOCKAPP_SIZE - 1, DOCKAPP_SIZE - 1, DOCKAPP_SIZE - 1);
		XDrawLine(dpy, pixmap, gc, DOCKAPP_SIZE - 1, 0, DOCKAPP_SIZE - 1, DOCKAPP_SIZE - 1);

		/* Clock dial circle */
		XSetForeground(dpy, gc, col_dial_bg);
		XFillArc(dpy, pixmap, gc, CLOCK_CX - CLOCK_R, CLOCK_CY - CLOCK_R,
			 CLOCK_R * 2, CLOCK_R * 2, 0, 360 * 64);
		XSetForeground(dpy, gc, col_bevel_lo);
		XDrawArc(dpy, pixmap, gc, CLOCK_CX - CLOCK_R, CLOCK_CY - CLOCK_R,
			 CLOCK_R * 2, CLOCK_R * 2, 0, 360 * 64);

		/* Hour tick marks */
		XSetForeground(dpy, gc, col_ticks);
		for (i = 0; i < 12; i++) {
			double angle = (double)i * (M_PI / 6.0);
			int x1 = CLOCK_CX + (int)(sin(angle) * (CLOCK_R - 2));
			int y1 = CLOCK_CY - (int)(cos(angle) * (CLOCK_R - 2));
			int x2 = CLOCK_CX + (int)(sin(angle) * (CLOCK_R - (i % 3 == 0 ? 5 : 3)));
			int y2 = CLOCK_CY - (int)(cos(angle) * (CLOCK_R - (i % 3 == 0 ? 5 : 3)));
			XDrawLine(dpy, pixmap, gc, x1, y1, x2, y2);
		}

		/* Hour hand */
		double h_angle = ((tm->tm_hour % 12) + tm->tm_min / 60.0) * (M_PI / 6.0);
		int hx = CLOCK_CX + (int)(sin(h_angle) * (CLOCK_R * 0.55));
		int hy = CLOCK_CY - (int)(cos(h_angle) * (CLOCK_R * 0.55));
		XSetForeground(dpy, gc, col_hour);
		XDrawLine(dpy, pixmap, gc, CLOCK_CX, CLOCK_CY, hx, hy);
		XDrawLine(dpy, pixmap, gc, CLOCK_CX + 1, CLOCK_CY, hx, hy);

		/* Minute hand */
		double m_angle = (tm->tm_min + tm->tm_sec / 60.0) * (M_PI / 30.0);
		int mx = CLOCK_CX + (int)(sin(m_angle) * (CLOCK_R * 0.80));
		int my = CLOCK_CY - (int)(cos(m_angle) * (CLOCK_R * 0.80));
		XSetForeground(dpy, gc, col_min);
		XDrawLine(dpy, pixmap, gc, CLOCK_CX, CLOCK_CY, mx, my);

		/* Second hand */
		double s_angle = tm->tm_sec * (M_PI / 30.0);
		int sx = CLOCK_CX + (int)(sin(s_angle) * (CLOCK_R * 0.88));
		int sy = CLOCK_CY - (int)(cos(s_angle) * (CLOCK_R * 0.88));
		XSetForeground(dpy, gc, col_sec);
		XDrawLine(dpy, pixmap, gc, CLOCK_CX, CLOCK_CY, sx, sy);

		/* Center pivot pin */
		XSetForeground(dpy, gc, col_text);
		XFillArc(dpy, pixmap, gc, CLOCK_CX - 2, CLOCK_CY - 2, 4, 4, 0, 360 * 64);

		/* Date text at bottom */
		strftime(date_buf, sizeof(date_buf), "%a %d %b", tm);
		XSetForeground(dpy, gc, col_text);
		XDrawString(dpy, pixmap, gc, 6, DOCKAPP_SIZE - 4, date_buf, (int)strlen(date_buf));

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

		/* Sleep until next second */
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
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
