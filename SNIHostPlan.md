# System Tray / StatusNotifierItem in the Dock — Implementation Plan

*Plan for wishlist item 5 of `ForTheFuture.md`. Written 2026-11-06; no code written yet.*

## Goal

Let the dock show modern freedesktop **StatusNotifierItem** applications (chat, audio, cloud sync)
natively, instead of relying on external `wmsystemtray`/`stalonetray` dockapps.

- We are the **StatusNotifierWatcher** (the host): we advertise ourselves on the session bus and
  accept `StatusNotifierItem`s.
- Each registered item becomes a **synthetic `WAppIcon`** in the dock: icon, tooltip, left-click →
  `Activate`, right-click → `ContextMenu`, wheel → `Scroll`.
- Optional phase 2: also host the legacy **XEmbed System Tray** (`_NET_SYSTEM_TRAY_S0`) for apps
  that still use the old protocol.

## Dependency

Use **libdbus** (`dbus-1`), not GDBus:

- GDBus needs glib's `GMainLoop`, which is separate from WINGs' own `poll()` loop in
  `WINGs/handlers.c` (`W_HandleInputEvents`, `total_fds`); integrating them means a second loop or
  manual `g_main_context` iteration.
- libdbus gives a plain fd: `dbus_connection_get_unix_fd()`. We add that fd to the existing
  `poll()` and call `dbus_connection_dispatch()` when it is readable — a natural fit for the
  current event loop, no threads, no glib main loop.

`configure.ac`: `PKG_CHECK_MODULES(DBUS, dbus-1)` behind `--enable-sni` (default off, to keep the
base build dependency-free). `src/Makefile.am`: compile `snihost.c` and link `@DBUS_CFLAGS@
@DBUS_LIBS@` only when enabled.

## Architecture

New files `src/snihost.c` / `src/snihost.h`, kept self-contained so the core WM only calls two
functions:

- `Bool wSnihostInit(WScreen *scr)` — connect to `DBUS_BUS_SESSION`, request the name
  `org.kde.StatusNotifierWatcher`, register match rules for
  `org.freedesktop.DBus.NameOwnerChanged`, and read the already-registered items.
- `void wSnihostShutdown(WScreen *scr)` — remove every SNI `WAppIcon`, drop the bus name, close the
  connection.
- `int wSnihostFd(void)` — expose the dbus fd so the main loop can poll it.
- `void wSnihostDispatch(void)` — called from the event loop when the fd is readable.

Called from `wScreenInit`/`wScreenDestroy` (or from `main.c` around the screen setup) and from the
`W_HandleInputEvents` poll loop, in the same place other fds are handled.

## StatusNotifier protocol surface

Only the parts we need:

- **Watcher** (`org.kde.StatusNotifierWatcher`): property `RegisteredStatusNotifierItems`,
  signals `StatusNotifierItemRegistered` / `StatusNotifierItemUnregistered`,
  `IsStatusNotifierHostRegistered`, `RegisterStatusNotifierItem`.
- **Item** (`org.kde.StatusNotifierItem`): properties `Title`, `IconName`, `IconPixmap` (ARGB
  ints), `OverlayIconPixmap`, `AttentionIconName`, `ToolTip` (icon+title+text), `Menu` (DBusMenu
  object path), methods `Activate(x,y)`, `ContextMenu(x,y)`, `SecondaryActivate(x,y)`, `Scroll(dx,dy)`.
- **Signals**: `NewIcon`, `NewToolTip`, `NewTitle`, `NewStatus`, `NewAttentionIcon`.

Phase 1 covers `IconName`/`IconPixmap`, `ToolTip`, `Title`, `Activate`, `ContextMenu` (best-effort)
and `Scroll`. DBusMenu (`com.canonical.dbusmenu`) is deferred: without it, right-click can fall back
to a simple menu with the item `Title`.

## Mapping to the dock

- An SNI item becomes a `WAppIcon` via the existing `wAppIconCreateForDock(scr, NULL,
  <service>, <bus_name>, TILE_NORMAL)` (`appicon.c:108`). `command` stays NULL (the icon is not a
  launchable app), so the dock's "launch" path must be skipped for these icons.
- Add a flag `sni:1` to `WAppIcon` (and `appicon.h`) so `updateDockMenu()`/`dockMenuCreate()` and
  the mouse handler can branch: `Activate`/`ContextMenu`/`Scroll` instead of launch/kill.
- Icon data: `IconPixmap` arrives as ARGB ints; convert to an `RImage` (wrlib already loads PNG via
  `RLoadImage`, so `IconName` resolution can go through the icon theme or a small name→file lookup)
  and set it on the `WIcon`. This needs a small `icon_create_from_image()` helper next to
  `icon_create_for_dock()`, because the current one only loads from a file.
- Mouse handlers: reuse `appIconMouseDown`/`iconMouseDown` but dispatch to `Activate` for button 1,
  `ContextMenu` for button 3, `Scroll` for wheel.

## Files to touch

- `configure.ac` — `--enable-sni` + `PKG_CHECK_MODULES(DBUS, dbus-1)`.
- `src/Makefile.am` — conditional `snihost.c` and `@DBUS_CFLAGS@ @DBUS_LIBS@`.
- `src/snihost.c`, `src/snihost.h` — new module.
- `src/appicon.h` — `sni:1` flag (+ an `item`/`dbus` handle if needed).
- `src/appicon.c` — `icon_create_from_image()` helper; skip launch for `sni` icons.
- `src/dock.c` — add SNI icons to the dock; branch menu/mouse handlers on `sni`.
- `src/main.c` / `src/screen.c` — call `wSnihostInit`/`wSnihostShutdown`; wire fd into the poll loop
  (`WINGs/handlers.c` `W_HandleInputEvents`).
- `src/event.c` — call `wSnihostDispatch()` when the dbus fd is readable.

## Commit plan (small, incremental, English, `Made-by-IA` trailer)

1. `configure.ac` + `src/Makefile.am`: add `--enable-sni` and libdbus detection (build passes with
   and without the flag).
2. `snihost.c`: connect, own `org.kde.StatusNotifierWatcher`, log `StatusNotifierItemRegistered`
   / `Unregistered` only (observable with `dbus-monitor`).
3. Create/destroy a synthetic `WAppIcon` per item (no real icon yet; placeholder tile), with the
   `sni:1` flag and the menu/mouse branch.
4. Real icon rendering: `IconPixmap`/`IconName` → `RImage` → `WIcon` (via `icon_create_from_image`).
5. Interaction: `Activate` on left-click, `ContextMenu` (fallback simple menu) on right-click,
   `Scroll` on wheel.
6. Cleanup: `wSnihostShutdown` in `wScreenDestroy`, `wSnihostDispatch` in the poll loop.
7. (Phase 2, optional) XEmbed System Tray `_NET_SYSTEM_TRAY_S0`.

## Verification

- `./autogen.sh && ./configure --enable-sni && make -j` — 0 warnings, both with and without
  `--enable-sni`.
- `make exit-test` and `EXIT_TEST_ARGS=--valgrind make exit-test` — clean shutdown, no leaks from
  the SNI module.
- Isolated Xvfb (`:9x`, throw-away `$HOME`) with a small test client that registers a
  `StatusNotifierItem` (or `stalonetray`-style script), assert: icon appears in the dock, tooltip
  correct, left-click calls `Activate`, right-click shows the fallback menu, item removal removes
  the icon.
- `dbus-monitor` during registration/unregistration to confirm the watcher lifecycle.

## Risks

- **New dependency** (dbus-1) — gated behind `--enable-sni`, so the default build is unchanged.
- **Event-loop integration** — one extra fd in the existing `poll()`; low risk, mirrors how the
  signal pipe is already handled.
- **Icon conversion** — ARGB ints → `RImage` is straightforward; `IconName` theme lookup is the
  fiddly part, fallback to `IconPixmap`.
- **DBusMenu** — the only genuinely complex piece; deferred to a later phase with a simple fallback.
- **XEmbed legacy tray** — separate phase; do not let it block the SNI-only first release.

## Non-goals (kept out on purpose)

- No compositing, no transparency (wishlist item 4).
- No full DBusMenu tree navigation in phase 1.
- No XEmbed System Tray in phase 1.

---

## Architectural Refinement & Implementation Notes (Gemini Review — 2026-11-06)

Before writing code, five technical refinements were identified by inspecting the exact call sites in `WINGs/` and `src/`:

1. **Event-Loop Integration via `WMAddInputHandler()` (zero changes to `WINGs/`)**:
   - There is no need to touch `WINGs/handlers.c` (`W_HandleInputEvents`). WINGs already provides the public `WMAddInputHandler(fd, WIReadMask, proc, clientData)` and `WMDeleteInputHandler()` APIs (used by `inotify` in `src/event.c:464` and the signal self-pipe in `src/startup.c:580`).
   - When integrating `libdbus` with an external `poll()` loop, register watch functions (`dbus_connection_set_watch_functions`) or monitor `dbus_connection_get_unix_fd()`, and drain the internal queue completely after `dbus_connection_read_write(conn, 0)`:
     ```c
     while (dbus_connection_get_dispatch_status(conn) == DBUS_DISPATCH_DATA_REMAINS)
         dbus_connection_dispatch(conn);
     ```
     Otherwise, if two D-Bus messages arrive in a single socket read, the second message stays buffered inside `libdbus` while `poll()` no longer sees readable bytes on the file descriptor.

2. **Exclude Synthetic `sni` Icons from `dockSaveState()` (`src/dock.c`)**:
   - In `src/dock.c:1604`, `dockSaveState()` iterates over `dock->icon_array[i]` and writes every icon where `!btn->attracted` into `~/GNUstep/Defaults/WMState`.
   - If `btn->sni` is not filtered out, temporary tray icons will be persisted to `WMState` on exit or session save and reappear as dead, commandless ghost tiles on the next startup.
   - Guard in `dockSaveState()`:
     ```c
     if (!btn || btn->attracted || btn->sni)
         continue;
     ```

3. **Non-Blocking / Asynchronous D-Bus Calls to Tray Clients**:
   - If a tray client (e.g., an Electron or Python app) freezes or hangs, calling `dbus_connection_send_with_reply_and_block()` from the WM to read properties (`IconPixmap`, `Title`, `ToolTip`) or invoke methods (`Activate`, `ContextMenu`, `Scroll`) would freeze the entire Window Manager for the D-Bus timeout (up to 25 seconds).
   - Action calls (`Activate`, `ContextMenu`, `SecondaryActivate`, `Scroll`) must be sent fire-and-forget (`dbus_message_set_no_reply(msg, TRUE)` + `dbus_connection_send()`).
   - Property queries (`GetAll` / `Get`) must use asynchronous `dbus_connection_send_with_reply()` with `DBusPendingCall` and a completion callback (`dbus_pending_call_set_notify`), or a strict short timeout if synchronous during test harness runs.

4. **`IconPixmap` `a(iiay)` Network Byte Order (Big-Endian ARGB32)**:
   - The freedesktop `StatusNotifierItem` specification defines `IconPixmap` as `a(iiay)`: an array of `(width, height, bytes)` where each pixel is 4 bytes in **big-endian ARGB32** (`[A, R, G, B]` byte order, regardless of CPU endianness).
   - `wrlib` `RImage` (`RCreateImage(w, h, True)`) stores RGBA bytes in `[R, G, B, A]` order. Conversion reads each 4-byte sequence `[b0, b1, b2, b3]` as `A=b0, R=b1, G=b2, B=b3` and writes `[R, G, B, A]`, picking the pixmap entry closest to `wPreferences.icon_size` and scaling via `RSmoothScaleImage()` when dimensions differ.

5. **Dual Watcher + Host Advertisement on `org.kde.StatusNotifierWatcher`**:
   - Many SNI clients check both that `org.kde.StatusNotifierWatcher` is owned on the session bus *and* that its property `IsStatusNotifierHostRegistered` is `true` (and listen for the `StatusNotifierHostRegistered` signal).
   - Since `snihost.c` acts as both Watcher and Host, it will always return `true` for `IsStatusNotifierHostRegistered`, `0` (int32) for `ProtocolVersion`, and emit `StatusNotifierHostRegistered` immediately upon acquiring the bus name.
