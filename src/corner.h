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

#ifndef WMCORNER_H_
#define WMCORNER_H_

#include "WindowMaker.h"
#include "screen.h"

enum {
	CORNER_NONE = 0,
	CORNER_TOPLEFT = 1,
	CORNER_TOPRIGHT = 2,
	CORNER_BOTTOMLEFT = 3,
	CORNER_BOTTOMRIGHT = 4
};

void wCornerHandleMotion(WScreen *scr, int x, int y);
void wCornerReset(WScreen *scr);
void wCornerExecuteAction(WScreen *scr, const char *action, int x, int y);

#endif /* WMCORNER_H_ */
