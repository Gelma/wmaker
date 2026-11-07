# SNI System Tray Host — Review and Improvements

*Review of the `StatusNotifierItem` dock host (SpecAI §142), plus the improvements applied on
2026-11-07. Evidence-based: source reading + rebuild + Valgrind + an isolated Xvfb with a real
D-Bus session bus.*

## Verdict

Solid, ready for a desktop smoke test. The module is self-contained, the watcher protocol is
correct, and the shutdown path is clean.

## What was measured

| Check | Result |
|---|---|
| `--enable-sni` build (gcc 14.2, `-std=gnu23`) | 0 errors / 0 warnings |
| `make exit-test` and `EXIT_TEST_ARGS=--valgrind make exit-test` | pass |
| Watcher on a real `dbus-run-session` bus | `Introspect` and `Properties.Get` answer correctly |
| `RegisterStatusNotifierItem` | `method return` |
| SNI icon detach on shutdown (item registered, Valgrind memcheck) | no `Invalid`, no SIGABRT, no new leak |

## Strengths

- Self-contained module (`snihost.c`/`snihost.h` behind `#ifdef USE_SNI`); the core only calls
  `wSnihostInit`/`wSnihostShutdown`.
- Complete watcher/item protocol: `Introspect`, `Get`/`GetAll`, `RegisterStatusNotifierItem/Host`,
  `NameOwnerChanged` tracking, and the `NewIcon`/`NewToolTip`/`NewStatus`/`NewAttentionIcon`
  signals.
- libdbus integrated into the existing WINGs `poll()` loop via `WMAddInputHandler(fd, ...)` — no
  `GMainLoop`, no threads.
- Clean shutdown: `wSnihostShutdown()` frees items, connection and `host_bus_name`, and the icon
  detach no longer trips `wDockDetach`'s assertion.
- Gated behind `--enable-sni` (default off); the base build is unchanged.

## Improvement points — applied (2026-11-07)

### 1. `sni_state` singleton documented (review point 4)

Added an explicit comment on `static SNIHostState *sni_state` explaining that the tray is a
per-session singleton bound to screen 0 and that other screens must not create a second host.

### 2. Icon-name resolution cached (review point 2)

`sni_resolve_icon_name()` used to re-run the full candidate search (dozens of `access()` +
`RLoadImage` attempts across theme paths, `icon_path` and hicolor/pixmaps) on every property
refresh. It now:

- takes the `SNIItem` and caches the last resolved name and path in
  `item->resolved_icon_name` / `item->resolved_icon_path`, so an unchanged `IconName` reuses the
  path directly instead of re-searching;
- orders the icon categories with `status` first (tray icons are almost always status-type);
- drops the rarely-used `.tiff` extension from the search.

The two new fields are freed in `sni_free_item()`. Build stays 0 warnings and
`exit-test --valgrind` passes after the change.

## Remaining improvement points (not applied)

- `dbus_shutdown()` in `wSnihostShutdown()` is a global libdbus call — fine while only `snihost`
  uses D-Bus, but it should be removed/circumscribed if other D-Bus code is added later.
- `wDockFindFreeSlot()` gained a new search path for `WM_DOCK`; the normal (non-SNI) icon
  placement should be spot-checked on a real desktop with many icons.
- `sni_extract_icon_pixmap()` and the tooltip parser are hand-written and already bounded; keep
  them under test with real clients.

## Not yet exercised (needs a real SNI app)

- Icon rendering from a real `IconName` (the synthetic test client only returned `IconPixmap`;
  the `IconName`→`RImage` path is implemented but untested end-to-end).
- `Activate`/`ContextMenu`/`Scroll` against a real app (`nm-applet`, `pasystray`).
- Multiple concurrent items and service-name collisions.
