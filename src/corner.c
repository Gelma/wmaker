/*
 *  Window Maker window manager
 *
 *  Hot corner state machine with hysteresis and cooldown
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

#include <time.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>

#include "corner.h"
#include "window.h"
#include "xinerama.h"
#include "menu.h"
#include "rootmenu.h"
#include "switchmenu.h"
#include "workspace.h"
#include "actions.h"
#include "main.h"

static uint64_t get_monotonic_ms(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
		return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
	return 0;
}

static inline int get_corner(WMRect rect, WMPoint p)
{
	const int edge = wPreferences.hot_corner_edge;
	const int left_edge = rect.pos.x + edge;
	const int right_edge = rect.pos.x + rect.size.width - edge;
	const int top_edge = rect.pos.y + edge;
	const int bottom_edge = rect.pos.y + rect.size.height - edge;

	int in_left = (p.x <= left_edge);
	int in_right = (p.x >= right_edge);
	int in_top = (p.y <= top_edge);
	int in_bottom = (p.y >= bottom_edge);

	if (!(in_left || in_right) || !(in_top || in_bottom))
		return CORNER_NONE;

	if (in_left && in_top)
		return CORNER_TOPLEFT;
	if (in_right && in_top)
		return CORNER_TOPRIGHT;
	if (in_left && in_bottom)
		return CORNER_BOTTOMLEFT;
	if (in_right && in_bottom)
		return CORNER_BOTTOMRIGHT;

	return CORNER_NONE;
}

void wCornerExecuteAction(WScreen *scr, const char *action, int x, int y)
{
	if (!action || !*action || strcasecmp(action, "NONE") == 0)
		return;

	if (strcasecmp(action, "RootMenu") == 0 ||
	    strcasecmp(action, "OpenApplicationsMenu") == 0) {
		OpenRootMenu(scr, x, y, False);
	} else if (strcasecmp(action, "CloseRootMenu") == 0) {
		if (scr->root_menu && scr->root_menu->flags.mapped)
			wMenuUnmap(scr->root_menu);
	} else if (strcasecmp(action, "WindowListMenu") == 0 ||
		   strcasecmp(action, "OpenWindowListMenu") == 0) {
		OpenSwitchMenu(scr, x, y, False);
	} else if (strcasecmp(action, "CloseWindowListMenu") == 0) {
		if (scr->switch_menu && scr->switch_menu->flags.mapped)
			wMenuUnmap(scr->switch_menu);
	} else if (strcasecmp(action, "CloseMenu") == 0) {
		if (scr->root_menu && scr->root_menu->flags.mapped)
			wMenuUnmap(scr->root_menu);
		if (scr->switch_menu && scr->switch_menu->flags.mapped)
			wMenuUnmap(scr->switch_menu);
	} else if (strcasecmp(action, "WorkspaceNext") == 0 ||
		   strcasecmp(action, "NextWorkspace") == 0) {
		wWorkspaceRelativeChange(scr, 1);
	} else if (strcasecmp(action, "WorkspacePrev") == 0 ||
		   strcasecmp(action, "PrevWorkspace") == 0) {
		wWorkspaceRelativeChange(scr, -1);
	} else if (strcasecmp(action, "ToggleShade") == 0 ||
		   strcasecmp(action, "Shade") == 0) {
		if (scr->focused_window) {
			if (scr->focused_window->flags.shaded)
				wUnshadeWindow(scr->focused_window);
			else
				wShadeWindow(scr->focused_window);
		}
	} else if (strcasecmp(action, "MinimizeAll") == 0 ||
		   strcasecmp(action, "HideAll") == 0 ||
		   strcasecmp(action, "ShowDesktop") == 0) {
		wHideAll(scr);
	} else {
		ExecuteShellCommand(scr, action);
	}
}

static void cornerTimerCallback(void *data)
{
	WScreen *scr = (WScreen *)data;

	scr->hot_corner_timer = NULL;
	if (scr->flags.in_hot_corner != CORNER_NONE) {
		const char *action = wPreferences.hot_corner_actions[scr->flags.in_hot_corner - 1];

		scr->flags.hot_corner_triggered = 1;
		if (action)
			wCornerExecuteAction(scr, action, scr->hot_corner_last_x, scr->hot_corner_last_y);
	}
}

void wCornerReset(WScreen *scr)
{
	if (!scr)
		return;

	if (scr->hot_corner_timer) {
		WMDeleteTimerHandler(scr->hot_corner_timer);
		scr->hot_corner_timer = NULL;
	}
	scr->flags.in_hot_corner = CORNER_NONE;
	scr->flags.hot_corner_triggered = 0;
	scr->hot_corner_cooldown_until = 0;
}

void wCornerHandleMotion(WScreen *scr, int x, int y)
{
	WMPoint p;
	WMRect rect;
	int corner;

	if (!scr || !wPreferences.hot_corners)
		return;

	scr->hot_corner_last_x = x;
	scr->hot_corner_last_y = y;

	p = wmkpoint(x, y);
	rect = wGetRectForHead(scr, wGetHeadForPoint(scr, p));
	corner = get_corner(rect, p);

	if (corner == CORNER_NONE) {
		if (scr->flags.in_hot_corner != CORNER_NONE) {
			/* Cursor left the corner */
			if (scr->flags.hot_corner_triggered) {
				const char *leave_act;

				leave_act = wPreferences.hot_corner_leave_actions[scr->flags.in_hot_corner - 1];
				if (leave_act)
					wCornerExecuteAction(scr, leave_act, x, y);

				scr->flags.hot_corner_triggered = 0;
				if (wPreferences.hot_corner_cooldown > 0)
					scr->hot_corner_cooldown_until = get_monotonic_ms() +
						(uint64_t)wPreferences.hot_corner_cooldown;
			}

			if (scr->hot_corner_timer) {
				WMDeleteTimerHandler(scr->hot_corner_timer);
				scr->hot_corner_timer = NULL;
			}
			scr->flags.in_hot_corner = CORNER_NONE;
		}
	} else {
		uint64_t now;

		if (scr->flags.in_hot_corner == (unsigned int)corner)
			return; /* Still inside the same corner */

		now = get_monotonic_ms();
		if (scr->hot_corner_cooldown_until > 0 && now < scr->hot_corner_cooldown_until)
			return; /* Cooldown active: ignore rapid bounce */

		/* Leaving previous corner if any */
		if (scr->flags.in_hot_corner != CORNER_NONE) {
			if (scr->hot_corner_timer) {
				WMDeleteTimerHandler(scr->hot_corner_timer);
				scr->hot_corner_timer = NULL;
			}
			scr->flags.hot_corner_triggered = 0;
		}

		scr->flags.in_hot_corner = (unsigned int)corner;
		scr->flags.hot_corner_triggered = 0;
		scr->hot_corner_cooldown_until = 0;

		if (wPreferences.hot_corner_delay > 0)
			scr->hot_corner_timer = WMAddTimerHandler(wPreferences.hot_corner_delay,
								  cornerTimerCallback, scr);
		else
			cornerTimerCallback(scr);
	}
}
