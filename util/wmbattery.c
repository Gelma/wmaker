/*
 *  Window Maker battery & power monitor dockapp
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
#include <dirent.h>
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

typedef struct {
	int present;
	int capacity;
	int is_charging;
	char status[32];
} BatteryInfo;

static int read_battery(BatteryInfo *info)
{
	DIR *dir;
	struct dirent *ent;
	char path[512];
	FILE *fp;

	info->present = 0;
	info->capacity = 100;
	info->is_charging = 0;
	strcpy(info->status, "AC");

	dir = opendir("/sys/class/power_supply");
	if (!dir)
		return -1;

	while ((ent = readdir(dir)) != NULL) {
		if (strncmp(ent->d_name, "BAT", 3) != 0)
			continue;

		snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity", ent->d_name);
		fp = fopen(path, "r");
		if (fp) {
			if (fscanf(fp, "%d", &info->capacity) == 1)
				info->present = 1;
			fclose(fp);
		}

		snprintf(path, sizeof(path), "/sys/class/power_supply/%s/status", ent->d_name);
		fp = fopen(path, "r");
		if (fp) {
			if (fgets(info->status, sizeof(info->status), fp)) {
				char *nl = strchr(info->status, '\n');
				if (nl)
					*nl = '\0';
				if (strcasecmp(info->status, "Charging") == 0)
					info->is_charging = 1;
			}
			fclose(fp);
		}

		if (info->present)
			break;
	}
	closedir(dir);

	return info->present ? 0 : -1;
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
	unsigned long col_bat_body, col_green, col_yellow, col_red, col_bolt;
	int delay_ms = 2000;
	const char *display_name = NULL;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-display") == 0 && i + 1 < argc)
			display_name = argv[++i];
		else if (strcmp(argv[i], "-delay") == 0 && i + 1 < argc)
			delay_ms = atoi(argv[++i]);
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("Usage: wmbattery [-display <display>] [-delay <ms>]\n");
			return 0;
		}
	}

	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);

	dpy = XOpenDisplay(display_name);
	if (!dpy) {
		fprintf(stderr, "wmbattery: unable to open display %s\n", XDisplayName(display_name));
		return 1;
	}

	screen = DefaultScreen(dpy);
	cmap = DefaultColormap(dpy, screen);

	col_bg = alloc_color(dpy, cmap, "#2c2c2c", "gray20");
	col_bevel_hi = alloc_color(dpy, cmap, "#545454", "gray40");
	col_bevel_lo = alloc_color(dpy, cmap, "#141414", "black");
	col_text = alloc_color(dpy, cmap, "#ffffff", "white");
	col_bat_body = alloc_color(dpy, cmap, "#181818", "black");
	col_green = alloc_color(dpy, cmap, "#33cc44", "green");
	col_yellow = alloc_color(dpy, cmap, "#ddcc33", "yellow");
	col_red = alloc_color(dpy, cmap, "#ee3333", "red");
	col_bolt = alloc_color(dpy, cmap, "#ffee33", "yellow");

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
		class_hint->res_name = "wmbattery";
		class_hint->res_class = "DockApp";
		XSetClassHint(dpy, win, class_hint);
		XFree(class_hint);
	}

	XSelectInput(dpy, win, ExposureMask | StructureNotifyMask);
	XSelectInput(dpy, icon_win, ExposureMask | StructureNotifyMask);

	XMapWindow(dpy, win);

	while (running) {
		BatteryInfo bat;
		char text_buf[32];
		int bat_x = 10;
		int bat_y = 19;
		int bat_w = 40;
		int bat_h = 22;

		read_battery(&bat);

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

		/* Header title */
		if (bat.present)
			snprintf(text_buf, sizeof(text_buf), "BAT: %3d%%", bat.capacity);
		else
			snprintf(text_buf, sizeof(text_buf), "AC POWER");
		XSetForeground(dpy, gc, col_text);
		XDrawString(dpy, pixmap, gc, 8, 13, text_buf, (int)strlen(text_buf));

		/* Battery frame */
		XSetForeground(dpy, gc, col_bat_body);
		XFillRectangle(dpy, pixmap, gc, bat_x, bat_y, bat_w, bat_h);

		XSetForeground(dpy, gc, col_bevel_lo);
		XDrawRectangle(dpy, pixmap, gc, bat_x, bat_y, bat_w, bat_h);
		/* Positive terminal nub on right */
		XFillRectangle(dpy, pixmap, gc, bat_x + bat_w + 1, bat_y + 6, 3, 10);

		if (bat.present) {
			int fill_w = ((bat.capacity * (bat_w - 4)) / 100);
			unsigned long fill_col = col_green;
			if (bat.capacity <= 15)
				fill_col = col_red;
			else if (bat.capacity <= 40)
				fill_col = col_yellow;

			if (fill_w > 0) {
				XSetForeground(dpy, gc, fill_col);
				XFillRectangle(dpy, pixmap, gc, bat_x + 2, bat_y + 2, fill_w, bat_h - 4);
			}

			if (bat.is_charging) {
				/* Draw lightning bolt */
				XPoint bolt_pts[] = {
					{ bat_x + 22, bat_y + 3 },
					{ bat_x + 14, bat_y + 12 },
					{ bat_x + 20, bat_y + 12 },
					{ bat_x + 18, bat_y + 19 },
					{ bat_x + 26, bat_y + 10 },
					{ bat_x + 20, bat_y + 10 }
				};
				XSetForeground(dpy, gc, col_bolt);
				XFillPolygon(dpy, pixmap, gc, bolt_pts, 6, Convex, CoordModeOrigin);
				XSetForeground(dpy, gc, col_bevel_lo);
				XDrawLines(dpy, pixmap, gc, bolt_pts, 6, CoordModeOrigin);
			}
		} else {
			/* No battery: draw plug icon / AC text inside */
			XSetForeground(dpy, gc, col_green);
			XDrawString(dpy, pixmap, gc, bat_x + 10, bat_y + 15, "AC ON", 5);
		}

		/* Bottom status text */
		if (bat.present) {
			if (bat.is_charging)
				snprintf(text_buf, sizeof(text_buf), "CHARGING");
			else if (bat.capacity == 100)
				snprintf(text_buf, sizeof(text_buf), "FULL");
			else
				snprintf(text_buf, sizeof(text_buf), "DISCHARGING");
		} else {
			snprintf(text_buf, sizeof(text_buf), "CONNECTED");
		}
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
