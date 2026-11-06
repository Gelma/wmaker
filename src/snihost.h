/* snihost.h - StatusNotifierItem (SNI) Watcher & Host for Window Maker Dock
 *
 *  Window Maker window manager
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

#ifndef WMSNIHOST_H_
#define WMSNIHOST_H_

#include "screen.h"
#include "appicon.h"

void wSnihostInit(WScreen *scr);
void wSnihostShutdown(WScreen *scr);

void wSnihostActivate(WAppIcon *aicon, int x, int y);
void wSnihostSecondaryActivate(WAppIcon *aicon, int x, int y);
void wSnihostContextMenu(WAppIcon *aicon, int x, int y);
void wSnihostScroll(WAppIcon *aicon, int delta, const char *orientation);
const char *wSnihostGetBalloonText(WAppIcon *aicon);

#endif /* WMSNIHOST_H_ */
