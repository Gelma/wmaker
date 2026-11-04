# DeepSeekCheck.md — Review of `d958afd39a..0c496c09` (HEAD)

Review date: 2026-11-03
Scope: 149 commits, 133 files, `+5,028 / −1,061` lines
Method: source review of every non-trivial hunk, `clang-23` full rebuild (project warning set),
Valgrind Memcheck runs, isolated Xvfb sessions (`:86`–`:99`, throw-away `$HOME`),
`test/wm_stress`, runtime telemetry dumps, and small hand-written X11 probes
(`XGrabKey`/`XTest`/`XSelectInput`) to verify grab and redirect behaviour.
The live user session was never touched.

> **Correction (2026-11-04).** The Valgrind runs quoted below were started through the libtool
> wrapper `src/wmaker`, which re-execs the real binary and therefore left the window manager
> *untraced*: those "clean" results described the monitor and the helper processes, not Window
> Maker itself. Re-instrumenting `src/.libs/wmaker --for-real` exposed a SIGSEGV on **every**
> shutdown (introduced by `d7035384`), found during the round-2 verification of the fix batch. See
> [UltimaRisposta.md](UltimaRisposta.md) and the appendix at the end of this file; the live status
> of each finding is maintained in [SpecAI.md](SpecAI.md).

---

## 1. Overall assessment

The batch is solid and clearly improves the tree: the window/application teardown now survives
a 12k-window churn plus a clean `SIGTERM` exit under Valgrind with **0 invalid reads/writes/frees**
and **0 "definitely lost"** bytes, the build is clean of errors with a strict warning set, the new
telemetry is genuinely useful, and the root-window key-grab refactor was verified to work
(F12 opens the root menu while a managed client has focus).

That said, the range contains a few *claims that are not (yet) true* and a handful of
small defects. The most important ones are metric/feature claims that future sessions will
trust (`xsync_eliminated`, `_NET_WM_SYNC_REQUEST`, signal-safe dump) and one
signal-safety regression that the range explicitly claims to have fixed.

Verified positives (measured, not assumed):

| Check | Result |
|---|---|
| Full rebuild (clang-23, project warning set) | 0 errors, 10 warnings (4 introduced here) |
| Valgrind, 12,841 windows created+destroyed | 0 invalid read/write/free, 0 double free |
| Valgrind, clean `SIGTERM` exit with clients open | 0 invalid ops, `definitely lost: 0` (all traced PIDs) |
| `wm_stress -t 4 -d 10` (4 threads, Xvfb) | 69,150 ops, 6,853 ops/s, WM alive afterwards |
| Telemetry JSON | valid, `.` decimals under `it_IT.UTF-8`, parses with Python/`jq` |
| `WM_RUNTIME_LOG_DIR` | honoured, files created, 60 s periodic dump works |
| Root key grabs | F12/Alt+m/Ctrl+Escape grabbed on root (+Lock/+NumLock variants); F12 opens the root menu with a client focused |
| App-icon / switch-panel `wretain`/`wrelease` | balanced on every path inspected |

---

## 2. Findings

Severity: **H** = wrong behaviour / unsafe, **M** = incorrect metric or latent hazard,
**L** = cosmetic / hygiene.

| # | Sev | Area | Finding |
|---|---|---|---|
| 1 | H | signals | `wTelemetryDump()` (and `Shutdown()`) still run *inside* the signal handler |
| 2 | H | EWMH | `_NET_WM_SYNC_REQUEST` is advertised but never awaited (`is_waiting` is dead) |
| 3 | H | strings | `ScreenCapture()` stack overflow with a long screenshot filename template |
| 4 | M | key grabs | Dedup key truncates the modifier to 8 bits (`AnyModifier` ≡ 0) |
| 5 | M | key grabs | Two full ungrab+regrab cycles per shortcut rebuild |
| 6 | M | telemetry | `xsync_eliminated` counts `XGrabServer()`; 83 % of the metric is fictional |
| 7 | M | telemetry | Duplicate JSON keys for extension events |
| 8 | M | security | Predictable `/tmp` paths (`wmaker_runtime_logs`, `.wmaker-exit-<pid>`) |
| 9 | M | KSM | Process-wide memory merging is on by default (clipboard data included) |
| 10 | M | teardown | Font-cache teardown breaks the hash enumerator contract and forces `refCount = 1` |
| 11 | M | refcount | `wApplicationDestroy()` re-saves a descriptor for a possibly freed `WWindow` |
| 12 | M | cache | `wWindowFor` L1 cache: measured 4.3 % hit rate, cost on every destroy |
| 13 | L | warnings | 4 new compiler warnings, all from leftovers of this batch |
| 14 | L | dead code | `W_FontShutdown`, `wRootMenuBindShortcuts`, `destroyBalloon`, `wWindowSetKeyGrabs`, `is_waiting` |
| 15 | L | duplication | Three copies of "is this window still managed"; three copies of grab-suspend logic |
| 16 | L | stress tool | Uninitialised `XSizeHints`, racy counters, `% (screen_width-w-20)` can divide by zero |
| 17 | L | git | 8 generated files still untracked (`.gitignore` gaps) |
| 18 | L | tooling | `wmaker-valgrind-session` defaults to `massif`, docs say Memcheck |
| 19 | L | docs | `SpecAI.md` self-contradictions and overstated claims (see §4) |

---

### 1. `DispatchEvent(NULL)` is still called from the signal handler — the dump is not signal-safe (H)

`src/startup.c:160` calls `DispatchEvent(NULL)` at the end of `handleExitSig()`. `DispatchEvent()`
now contains `wTelemetryDump()` (`src/event.c:195`) and `Shutdown()` for `WSTATE_NEED_EXIT`
(`src/event.c:184`). Both allocate memory (`wmalloc`, `wstrdup`), call `fopen()`/`fprintf()` and
do arbitrary library work.

So the statement in `SpecAI.md` §95 — *"Moved `wTelemetryDump()` … to `DispatchEvent()` in the
main event loop to avoid interrupting heap allocations"* — is not achieved: the code path is
`signal → handleExitSig → DispatchEvent(NULL) → wTelemetryDump()`, i.e. exactly the same
non-async-signal-safe context. `SIGUSR2` (request a defaults re-read) will exercise
`wDefaultsCheckDomains()` + telemetry from a signal handler as well; `SIGTERM` will run the whole
`Shutdown()` (X calls, `unlink`, `kill`, frees) there. The `sigprocmask()` around it prevents
re-entrant signals, but not `malloc`-in-signal-handler corruption/deadlock.

Suggested fix: the handler must only do `volatile sig_atomic_t pending = NEED_EXIT|NEED_REREAD|…`;
process it at the top of the main loop. To wake the loop from `select`/`poll`, either use the
existing self-pipe trick or add the signal fd to the `W_HandleInputEvents()` poll set.
This also removes the need for `SIG_WCHANGE_STATE()` (which writes a global state machine from
a signal handler).

### 2. `_NET_WM_SYNC_REQUEST` is advertised but the WM never waits (H)

`wmspec.c:1734-1772` sends `_NET_WM_SYNC_REQUEST` on every `wWindowConfigure()` and arms an
`XSyncAlarm` plus a 250 ms timer. `wwin->sync_request.is_waiting` is set in
`wmspec.c:1771`/cleared in `wmspec.c:1681` and `wmspec.c:1784` — `grep` shows **no reader
anywhere**: the WM never defers the frame update, never delays the resize loop, never uses the
acknowledgement for anything.

Advertising the hint means GTK/Qt clients *do* enable the protocol, keep a sync counter, and
perform the extra `XSyncSetCounter` work per frame — all cost, no benefit. Two honest options:

* remove `net_wm_sync_request` from `_NET_SUPPORTED` and drop `wNETWMSendSyncRequest()` (simplest,
  removes ~1 `XSendEvent` + alarm change + timer churn per configure), or
* implement it: on resize, send the request, set `is_waiting`, and in the alarm/timeout handler
  complete the pending frame update (only resize, not moves).

Also: `XSyncAlarmAttributes values;` (`wmspec.c:1748`) is not zero-initialised (only the three
flagged fields are assigned — safe today, fragile); `XSyncValueAdd()` overflow is ignored; and a
new 250 ms timer is created/removed on *every* configure, including moves.

### 3. `ScreenCapture()`: stack overflow with a custom filename template (H, pre-existing)

`src/screen.c:1278-1331`:

```c
char filename_date_part[60];
char filename[60];
strftime(filename_date_part, sizeof(filename_date_part), wPreferences.screenshot_filename_template, tm_info);
...
strcpy(filename, filename_date_part);
filepath = wstrconcat(screenshot_dir, strcat(filename, filetype));   /* + ".png" */
```

The template is user-configurable (`screenshot_filename_template`). A template that fills
`filename_date_part` (59 chars + NUL) makes `strcat(filename, ".png")` write 4 bytes past the
stack buffer. `strftime()`'s return value (0 on overflow, buffer contents then undefined) is
not checked either. Fix: check `strftime() == 0`, size `filename` as `sizeof(filename_date_part) + 8`,
and use `snprintf`/`wstrconcat` instead of `strcpy`+`strcat`.

This is technically older than the range, but it is exactly the class of defect this batch set out
to remove, and the whole file was touched here.

### 4. Key-grab dedup truncates the modifier to 8 bits (M)

`src/window.c:2862`:

```c
static void addSingleKeyGrab(KeyCode keycode, unsigned int mod)
{
        unsigned int m = mod & 0xFF;
        if (grab_seen[keycode][m >> 3] & (1U << (m & 7))) { ... return; }
        ...
        compiledKeyGrabs[compiledKeyGrabsCount].modifier = mod;   /* full value */
```

`AnyModifier` is `1 << 15`, so it lands in bucket `m == 0` and collides with plain "no modifier"
bindings. Depending on insertion order (`wKeyBindings[]` is walked before root-menu shortcuts),
a real grab is silently dropped — the shortcut then only works without modifiers, or not at all.
`key_grabs_deduplicated` was **0** in every run I made, i.e. the dedup never fires on real data;
it only introduces the collision risk.

Suggested fix: dedup on the full `(keycode, modifier)` pair (a 64k-bit bitset per keycode, or a
small open-addressed set of 32-bit pairs), or compare the full value before returning.

### 5. Two ungrab+regrab cycles per shortcut rebuild (M)

`wKeyTreeRebuild()` ends with `wScreenSetKeyGrabs(scr)` (`defaults.c:1487`), and every caller then
calls `rebindKeygrabs()` (`rootmenu.c:382`), which does `wWindowInvalidateKeyGrabs()` +
`wScreenSetKeyGrabs(scr)` again:

* `rootmenu.c:744`, `rootmenu.c:802` (`constructMenu`, `constructPLMenuFromPipe`),
* `rootmenu.c:1708` (`wRootMenuReparse`), `rootmenu.c:1825` (`OpenRootMenu`).

That is ~2 × 240 `XGrabKey` requests plus two `XUngrabKey(Any…)` per rebuild, and the root window
is briefly ungrab'd twice. `rebindKeygrabs()` should keep only the invalidate, or the callers
should call only one of the two. (Observed `key_grabs_set: 5260` over 60 s of stress, i.e. ~22
installs — twice the minimum.)

### 6. `xsync_eliminated` counts things that are not XSync (M)

`src/window.c:791-792`:

```c
/* mutex. */
XGrabServer(dpy);
wTelemetry.xsync_eliminated++;
```

Every window map increments the "XSync round-trips eliminated" metric. In my stress run the
counter reported 15,386 "eliminated round-trips" of which ≈12.8k (83 %) come from this single
line, not from a removed `XSync`. The human log labels it "Event Loop XSync Round-Trips
Eliminated", so the number is actively misleading.

Fix: drop that increment (a server grab is not an XSync), or rename the metric to
`blocking_calls_removed` / add a separate `server_grabs` counter. Same for `client.c:290`,
`actions.c:196`, `actions.c:1388`, `actions.c:1592` — decide whether `XFlush()` replacing `XSync()`
should count (it should, but say so in the name).

Also `key_grabs_set` counts *planned* grabs, not successful `XGrabKey` calls (a grab can fail with
`BadAccess` if another client owns the combination), and the log label "Colormap Queries / XSync
Bypassed" mixes two different counters.

### 7. Duplicate JSON keys for extension events (M)

`src/telemetry.c:206-224` writes one object per counted event type, keyed by
`get_event_name(i)`; the default branch (`telemetry.c:66`) returns `"ExtensionOrOther"` for every
type > `MappingNotify`. As soon as two different extension events are counted (XKB, XSync, XInput,
RANDR…), the JSON contains the same key twice: parsers keep the last one and the first counter is
lost, even though the object carries `"type": N`.

Fix: use a unique key, e.g. `"ExtensionOrOther_%d"`, or emit an array of objects.

### 9. `PR_SET_MEMORY_MERGE` is on by default, twice (M)

`src/main.c:646` and `WINGs/wapplication.c:32` both call
`prctl(PR_SET_MEMORY_MERGE, 1, ...)` — the second one is redundant. Two concerns:

* KSM on a window manager means window titles, `xdestring` (DND payload), `paste_command`
  and clipboard-related buffers participate in cross-process page deduplication. That is a
  well-known side channel; for data that may contain user content it should be opt-in.
* `wrlib/raster.c:92-115` additionally does `madvise(MADV_MERGEABLE)` on every image ≥ 4 KB
  (redundant with the process-wide setting) and tries `MAP_HUGETLB` for images ≥ 2 MB. Huge pages
  are a scarce, system-wide resource: a WM should not compete for `vm.nr_hugepages`. The
  anonymous-mmap + `MADV_HUGEPAGE` fallback is the right tool; the hugetlb attempt mostly adds an
  extra failing syscall per large image (and consumes reserved pages when they exist).

Suggestion: keep `MADV_HUGEPAGE`/4 KB alignment, drop `MAP_HUGETLB` and the per-image
`MADV_MERGEABLE`, and gate `PR_SET_MEMORY_MERGE` behind an env var (e.g. `WM_KSM=1`), documenting
the trade-off in `SpecAI.md`.

### 10. `WMDestroyScreen()` font-cache teardown is fragile (M)

`WINGs/widgets.c:1256-1263` (new in `00c0c04a`) enumerates `scr->fontCache` and calls
`WMReleaseFont()` — which does `WMHashRemove()` — *inside* the enumeration, and forces
`font->refCount = 1` first. `WMNextHashEnumeratorItem()` documents *"this assumes the table doesn't
change between … calls"* (`WINGs/hashtable.c`). It happens to work (removal does not rehash and the
enumerator already advanced to `nextItem`), and my Valgrind exit run was clean, but any future
shrinking rehash makes this a use-after-free. Forcing `refCount = 1` also frees fonts that are
still referenced elsewhere.

Fix: snapshot the fonts into a temporary `WMArray`, release them outside the enumeration, then
`WMFreeHashTable()`. Cheap and explicit.

### 11. `wApplicationDestroy()` re-saves a descriptor after `wWindowDestroy()` (M)

`src/application.c:189-199`:

```c
wwin = wWindowFor(wapp->main_window_desc->client_win);
wWindowDestroy(wapp->main_window_desc);          /* deletes client_win context (same XID!) */
if (wwin) {
        XSaveContext(dpy, wwin->client_win, ..., &wwin->client_descriptor);
}
```

This relies on `main_window_desc` and the *managed* `WWindow` being two different objects for the
same XID (`makeMainWindow()` creates a second `WWindow`), and on `wWindowFor()` returning the
managed one (or NULL). If the managed window was already destroyed, `wwin` is NULL and the context
stays deleted; if it is alive, the descriptor is restored — but `wWindowDestroy()` just invalidated
the new L1 cache for `main_window_desc`, not for `wwin`, so the invariant is only maintained by
accident.

Suggestion: make the intent explicit with a helper (`wWindowRestoreClientContext(window)`), add an
assertion/comment about the two-objects-one-XID invariant, and make the log cache invalidation
cover all entries whose `window` is the destroyed XID (belt and braces for the future).

### 12. `wWindowFor()` L1 cache: benefit not demonstrated (M)

Measured over 60 s of stress (`runtime_stats.json`): **3,023 hits vs 67,641 misses = 4.3 %**.
Each `wWindowDestroy()` also pays a 128-entry scan (`window.c:154-165`), i.e. ~1.66 M comparisons
for 13k destroyed windows — cheap, but the cache buys almost nothing on this (realistic) workload
while adding a second source of truth for window→descriptor mapping.

Options: (a) cache only the hot pairs (frame window → child) which are looked up repeatedly during
event bursts; (b) drop the cache; (c) keep it but only after showing a workload where the hit rate
is high, and add a compile-time switch so the counters disappear from the hot path.

Unrelated but adjacent: the cache counts hits/misses for *all* lookups, including lookups of
windows that never existed (destroyed windows), which lowers the reported rate — consider
separating "miss because not found" from "miss because evicted" so the metric can guide decisions.

### 13. New compiler warnings (L)

`make` with the project's strict clang set is warning-free except 10, **4 introduced by this
range**:

```
wrlib/gradient.c:47  nonnull parameter 'colors' will evaluate to 'true' on first encounter
WINGs/wballoon.c:42  unused function 'destroyBalloon'
src/xmodifier.c:87   variable 'ModeIndex' set but not used
src/xmodifier.hack_nf.c:87  variable 'ModeMask' set but not used
```

* `gradient.c:47`: the new `if (!colors) return NULL;` contradicts
  `__wrlib_nonnull` on `RRenderMultiGradient` (`wrlib/wraster.h.in`); clang says the check is dead.
  Keep the guard and remove the annotation, or drop the guard.
* `wballoon.c:42`: `00c0c04a` removed the `destroyBalloon()` definition but left the `static`
  prototype.
* `xmodifier.c`: the nested-function → macro conversion (`modwarn`/`modbarf`/`store_modifier`)
  replaced the last readers of the globals `ModeIndex`/`ModeMask` (the macro uses `mode_bit`).
  Delete them (and the now-unused `MetaMask`… if they follow the same fate).

The other 6 are pre-existing (`rotate.c`, `load_png.c`, `wbrowser.c`, `wlist.c`, `wmiv.c` ×2).

Suggestion: add `-Werror` (or `-Werror=<select>`) to the configure warning set, or a `make
warnings` target so "0 warnings" is a test, not a claim.

### 14. Dead code introduced (L)

* `W_FontShutdown()` (`WINGs/wfont.c:602`) — defined, never called (SpecAI §73/§318 still say
  `Exit()` calls it; §357 says it was removed).
* `wRootMenuBindShortcuts()` (`rootmenu.c:377`) — no callers after the grab refactor.
* `wWindowSetKeyGrabs()` (`window.c:2982`) — only a self-referential wrapper; no callers.
* `destroyBalloon` prototype (see above).
* `sync_request.is_waiting` (see finding 2).
* `if (wapp->main_window_desc && …)` NULL guards added in several places: `main_window_desc` is
  assigned once in `wApplicationCreate()` and never cleared, so the guard can never be false.
  The real hazard is a dangling pointer, not NULL — an explicit invalidation (or a `destroyed`
  flag) would be worth more than the guards.

### 15. Duplicated helpers (L)

* `wWindowIsManaged()` (`window.c:219`), `isManagedWindow()` (`cycling.c:58`),
  `windowStillManaged()` (`switchpanel.c:79`) are three copies of the same O(N) focus-list scan
  with slightly different return types. Use `wWindowIsManaged()` everywhere and drop the other two.
* Key-grab suspension is implemented three times with the same five lines:
  `window.c:1970-1981` (suspend + restore in `wWindowFocus()`), `actions.c:146-149` (restore when
  focus moves to `no_focus_win`) and `winspector.c:769-781` (both, on attribute change). Extract
  `void wScreenSetKeyGrabsSuspended(WScreen *scr, Bool suspend)` and call it from all three.
* `wRootMenuInsertIntoTree()` and `wKeyTreeRebuild()` repeat the "build mods/keys arrays, insert,
  free" block and differ in the NULL check after `wKeyTreeInsert()` (`rootmenu.c:357` checks,
  `defaults.c:1478` does not → `wKeyNodeAddAction(NULL, …)` would crash if `len == 0`).

### 16. `test/wm_stress.c` details (L)

* `XSizeHints hints;` is not zeroed (`test/wm_stress.c:160-168`). Only flagged fields are sent by
  Xlib, but ASAN/Valgrind-friendly code should `memset` it (same class of issue this range fixed
  elsewhere).
* Worker threads do `arg->created_count++`/`…++` (`wm_stress.c:178-241`) while the main thread
  reads them without atomics (`wm_stress.c:391-394`, `422-425`): a data race that TSAN/Helgrind
  will flag. Use `_Atomic` or a mutex.
* `sw->x = (fast_rand(&seed) % (arg->screen_width - sw->w - 20)) + 10;` divides by zero if
  `screen_width == w + 20`, and produces negative positions on narrow displays. Clamp the range.
* Cleanup destroys windows that may already be iconified by the WM; harmless for a test tool, but
  the summary counts will not match the WM's view. Say so in the header comment.
* `-lpthread` is hard-coded in `test/Makefile.am`; use `$(PTHREAD_LIBS)`/`AX_PTHREAD` for
  portability.
* `XInitThreads()` is unnecessary when each thread has its own `Display` (and does not make a
  single `Display` thread-safe). Either drop it with a comment, or document why it is there.

### 17. Git hygiene (L)

`git status` still shows 8 generated files as untracked:

```
WindowMaker/appearance.menu.fy   WindowMaker/appearance.menu.nl
WindowMaker/appearance.menu.sr   WindowMaker/menu.sr
WindowMaker/plmenu.sr           doc/wmaker.1
doc/wmsetbg.1                    wmlib/wmlib.pc
```

Add them to `.gitignore` (the existing rules cover binaries only). Keeping `git status` clean
makes the incremental-commit workflow reliable.

### 18. Tooling (L)

* `wmaker-valgrind-session` defaults to `TOOL=massif` while the comments above it (and SpecAI)
  describe Memcheck as the default. Pick one and align the comments.
* In the `massif` branch, `--pages-as-heap=yes` and `--alloc-fn=wmalloc,wrealloc,wstrdup` are used
  together: with `--pages-as-heap` Massif already accounts OS pages, so the per-function
  attribution becomes meaningless. Use `--pages-as-heap` *or* the alloc-fn list (for a WM that
  mmaps large images, `--pages-as-heap` is the interesting one).
* `dhat`/`massif` branches have no `--log-file`, so Valgrind's own errors end up mixed in the
  wmaker stdout log.

### 19. One runtime anomaly I could not reproduce (open question)

On the long Valgrind session (`:99`, ~8 min, 12k windows churned, plus several inotify
defaults reloads) the WM stopped adopting new clients:

* `MapRequest` telemetry incremented for the new window (the event *was* dispatched),
* the client window stayed `IsUnMapped` and a direct child of the root,
* `_NET_CLIENT_LIST` was empty while 80 root children (dock/appicons) still existed,
* `SubstructureRedirect` was still owned by the WM, and the WM answered inotify events.

That combination points at an early return in `handleMapRequest()` (`wWindowFor() != NULL`,
`src/event.c:643`) or at one of the three `return NULL` paths of `wManageWindow()`
(`window.c:794-807`, including the `scr->flags.startup && map_state == IsUnmapped` guard).
I could reproduce neither in clean 10 s runs nor with 8 s of stress on a fresh instance
(management stayed correct), so I am not calling it a regression — but it is exactly the kind of
state leak a soak test would catch.

Recommendation: add a debug counter/`wwarning` when `handleMapRequest()` returns because
`wWindowFor()` found an existing descriptor, and when `wManageWindow()` returns NULL; then run a
≥100k-window soak under ASAN. If the descriptor is stale, the L1 cache and the two-objects-one-XID
pattern (finding 11) are the first places to look.

---

## 3. Additional improvement ideas (not defects)

1. **Measurement over claims.** Several SpecAI entries assert speed-ups that are not measured
   (round-trip counts, "faster string helpers"). The telemetry infrastructure now exists: make it
   the evidence. Add before/after numbers to the spec (same workload, same machine) or phrase the
   claim as "design change" rather than "N million round-trips saved".
2. **`_NET_WM_SYNC_REQUEST` + `wWindowConfigure()`**: if the sync path is kept, only send requests
   for *resizes* (not moves), and skip the whole thing when the size did not change.
3. **`wScreenUpdateRootEventMask()`**: called from `wReadDefaults()` with
   `wPreferences.scrollable_menus || hot_corners`. Verify (multi-screen) that every screen is
   updated, not just the first one — today it is called per-screen only at init.
4. **`updateStrut()`** returns "changed" whenever the property *exists*, so a client that rewrites
   an identical strut still triggers the whole usable-area recalculation. Compare old/new areas and
   return `False` when they are equal (this is what SpecAI §92 promises). Also `updateStrut(adding)`
   appends duplicate `WReservedArea` entries if ever called twice without a delete — cheap to make
   idempotent.
5. **`MappingNotify`**: compiled grabs are never refreshed after an `xmodmap` change (keycodes and
   `_NumLockMask` are captured at startup). Handle `MappingKeyboard`/`MappingModifier` in
   `DispatchEvent()`: invalidate the compiled grabs, recompute keycodes, `wScreenSetKeyGrabs()` on
   every screen. With the new "install once per screen" design the stale window now lasts the whole
   session.
6. **Motion compression and timestamps**: `saveTimestamp(event)` runs before the drain, so
   `w_global.timestamp.last_event` is the *oldest* event of the batch, and drained events are not
   counted in telemetry. Save the timestamp of the last drained event (or document the choice).
7. **Telemetry growth**: `latency_spikes.log` is appended forever (one line per >16.6 ms event) and
   never rotated; cap it (e.g. keep the last N lines) or rotate at 1 MB.
8. **`wfont` caches**: the 65,536-entry width cache (~6 MB worst case) plus 2,048 Pango layouts is
   invalidated with an O(65,536) scan per released font and has no hit/miss counters. Add counters
   (like the `WWindow` cache) and size the cache from measurements; `WMWidthOfString()` is called
   with many one-off strings (balloons, titles) where the cache cannot help.
9. **`wrlib` 4-byte casts**: `RScaleImage()`/`r_flip_horizontally()` now read `image->data` through
   `uint32_t *`. That is fine for buffers produced by `RCreateImage()` (malloc/posix_memalign/mmap)
   but is not guaranteed for an arbitrary `RImage` handed in by a caller. A `__builtin_assume_aligned`
   or a comment makes the contract explicit (and keeps non-x86 ports honest).
10. **`wScreenDestroy()`**: the mini-screenshot window is destroyed when
    `scr->mini_screenshot_timeout != 0` (`screen.c:1665-1668`), and `hideMiniScreenshot()` destroys
    the window without resetting `scr->mini_screenshot = None`. It works today because the timeout
    doubles as the "window exists" flag, but a `None` check is clearer and avoids a possible
    `BadWindow` on teardown.
11. **`WMDestroyScreen()`** frees `scr` with `free()`; the file otherwise uses WINGs allocators —
    use `wfree()` for consistency (matters if a different allocator backend is configured).
12. **`wSetFocusTo()` / `ignore_focus_events`**: the `old_scr` static in `actions.c:124` is never
    reset when a screen is destroyed; with the new per-screen grab flag, review multi-screen
    teardown with two `WScreen`s (the flag is per screen, the focused window is global).

---

## 4. `SpecAI.md` corrections

The file is the memory of the project; a few statements are now inaccurate or contradict each other.

| SpecAI | Problem | Suggested text |
|---|---|---|
| §55/§95 "~70 synchronous X11 round-trips per window creation, over 2.3 M round-trips saved" | `XGrabKey` has no reply: they are *requests*, not round-trips. The WM still issues ~240 grab requests per install (once per screen). | "≈70 X11 *requests* per window creation eliminated (no reply was ever awaited); the new design issues one grab set per screen." |
| §95 "Moved `wTelemetryDump()` … to `DispatchEvent()` … to avoid interrupting heap allocations" | `handleExitSig()` still calls `DispatchEvent(NULL)`, so the dump runs in the handler (finding 1). | State clearly that the dump is *triggered* by the signal path and is still not async-signal-safe; or fix finding 1 and then reword. |
| §73/§318 vs §357 vs §396 (`W_FontShutdown`) | §73/§318 say `Exit()` calls `W_FontShutdown()`; §357 says the call was removed; §396 claims it guarantees 0 leaks. Today it is defined and never called. | One paragraph: "`FcFini()` is intentionally not called before `XCloseDisplay()`; `W_FontShutdown()` is currently unused." |
| §92 "Guarded all calls … behind whether struts were actually modified" | `updateStrut()` returns true when the property merely exists. | "…when a strut property is present/removed (not when its value actually changes)." |
| §87 "128-entry L1 … resolving in 2 ALU instructions (~1–2 ns)" | True per hit, but the measured hit rate is 4.3 % (3,023/70,664) under stress. | Add the measured hit rate and the invalidation cost, or drop the micro-benchmark wording. |
| §93 "guarantee standard period decimal points … ensuring RFC 8259 compliance" | Correct for the JSON file; the human log is written *after* the locale is restored and is locale-formatted (`60,00 s`), which is fine but should be stated. | "JSON is serialised under `LC_NUMERIC=C`; the text report keeps the user locale on purpose." |
| §87/§91 telemetry counters | `key_grabs_deduplicated` is always 0, `xsync_eliminated` is dominated by `XGrabServer`, `key_grabs_set` counts planned grabs. | Document each counter's exact semantics in `telemetry.h`. |

Note for future hardening entries: a grep at HEAD still finds **63 `sprintf`, 25 `strcpy`,
53 `strcat` and 12 `atoi`** outside `WPrefs.app` (e.g. finding 3). They are not all dangerous, but
if the spec ever claims "no unsafe string calls left", it must be qualified (or the remaining sites
listed as follow-up work).

Also worth recording in `SpecAI.md`: the measured baseline numbers from this review
(12.8k-window Valgrind run clean, 6,853 ops/s stress on 4 threads, 4.3 % cache hit rate,
`wm_stress` usage) so future sessions can compare instead of re-deriving.

---

## 5. Suggested test plan (cheap, high value)

1. `make check` currently builds nothing portable: `test/Makefile.am` defines no `TESTS`.
   Wire `wm_stress` (with `-d 5 -t 2`) and a headless smoke test into `make check`/CI using
   `xvfb-run`, so the "0 invalid ops" result is reproducible.
2. Add an automated Valgrind job for **exit**: `Xvfb` + wmaker + 2 clients + `SIGTERM`,
   `--error-exitcode=1 --leak-check=full --errors-for-leak-kinds=definite`. Today that passes;
   make it stay passing.
3. Add a **soak** test (≥100k windows, 10+ min) with ASAN, plus the debug counters from
   finding 19 — this is the only way to catch the state leak I observed.
4. Add a **key-grab regression test**: `XTest` F12 (root menu) and `Alt+m` (miniaturise) with a
   client focused, plus `MappingNotify` (xmodmap swap) and a `no_bind_keys` window.
5. Add a **defaults-reload test**: rewrite `~/GNUstep/Defaults/WindowMaker` (and `WMRootMenu`)
   repeatedly while mapping windows; assert no X errors and that shortcuts still work.
6. Run `helgrind`/TSAN on `wm_stress` itself (it is a test tool; its races make failure
   interpretation noisy).

---

## 6. Not verified / limits of this review

* Multi-screen (Xinerama/RandR): the key-grab flag is per screen but the focus window is global;
  I only tested single-head Xvfb.
* `no_bind_keys` suspension was not exercised end-to-end (needs a GNUstep client or the inspector);
  the code paths were reviewed only.
* `_NET_WM_SYNC_REQUEST` behaviour was reviewed statically; no GTK/Qt client was run to watch the
  counter. The `is_waiting` dead flag is conclusive on its own.
* The `RenderBadPicture (invalid Picture parameter)` X errors observed 4 times during the Valgrind
  stress run (request 139, minor 7 = `RenderFreePicture`) are **not** in the user's real session
  logs, so I did not classify them as a regression. They disappeared in clean runs; worth a
  dedicated Picture-lifetime check if they reappear.
* Performance numbers were taken under Valgrind/Xvfb on this machine; they are useful for
  *relative* comparison only.

---

## 7. Suggested priority

1. **Fix finding 1 (signal path)** — it is the only correctness issue with a realistic crash
   mechanism, and it is the one the batch claims to have fixed.
2. **Decide on finding 2** (implement or de-advertise `_NET_WM_SYNC_REQUEST`) — either way the
   spec gets honest and the per-configure overhead drops.
3. **Finding 3** (screenshot filename overflow) — small, user-triggerable, stack corruption.
4. **Findings 4, 5, 6, 7** — cheap code fixes, they make the grab/telemetry metrics trustworthy.
6. **Findings 9, 10, 11, 12** — review with the maintainer: they are policy/design choices.
7. **Findings 13–18** — cleanup pass, ideally in the same commit series as the next feature.
8. **Finding 19** — build the soak test; it either reproduces or rules out a state leak.

---

## Appendix — Round-2 verification (2026-11-04)

This appendix records the verification of the follow-up batch that implemented the fixes
(`d0dd9291..HEAD`, thirteen commits). The full narrative, with evidence per fix, is in
[UltimaRisposta.md](UltimaRisposta.md); the live status of every finding is in
[SpecAI.md](SpecAI.md).

### Verdicts

| Fix | Subject | Verdict |
|---|---|---|
| 1 | POSIX self-pipe for signals | correct; masked by the shutdown crash until `151ccd71` |
| 2 | de-advertise `_NET_WM_SYNC_REQUEST` | functional, ~40 lines of dead stubs left |
| 3 | `ScreenCapture()` bounds | correct; verified with a 240-character template |
| 4 | exact key-grab dedup | correct |
| 5 | remove double rebind | correct for menus; `setKeyGrab()` is still O(N²) (R1) |
| 6 | `xsync_eliminated` | correct; 478 instead of 15,386 |
| 7 | unique JSON keys | correct |
| 9 | KSM/HugeTLB | half solved: `MAP_HUGETLB` removed, KSM still default |
| 10 | font-cache snapshot | correct, but unreachable until `151ccd71` |
| 13 | compiler warnings | correct; 10 → 6 warnings (all pre-existing) |
| 15 | consolidated helpers | correct |
| 16 | `wm_stress` hardening | correct; `-lpthread` still hard-coded |
| 17 | `.gitignore` | works, but the patterns are too broad (R3) |
| 19 | `MapRequest` guard | does not address the observed anomaly (no diagnostics) |

### The crash the earlier review missed

With `valgrind src/.libs/wmaker --for-real`, every `SIGTERM`/`SIGUSR1` ended with

```
Invalid read of size 8 at address 0x8
  WMSetBalloonTextForView (wballoon.c:93)
  destroyView (wview.c:332)
  W_DestroyBalloon (wballoon.c:477)
  WMDestroyScreen (widgets.c:1195)
  wScreenDestroy (screen.c:1673)
  Shutdown (shutdown.c:100)
```

Root cause: `d7035384` moved `scr->balloon = NULL` *before* `W_DestroyBalloon()`, and
`destroyView()` calls `WMSetBalloonTextForView(NULL, view)`, which dereferences
`view->screen->balloon`. Exit status was 139 (SIGSEGV) in all cases, so `Exit()`,
`ExecExitScript()` and `wCleanupSignalPipe()` never ran. Fixed in `151ccd71` by making the balloon
API tolerate a detached screen balloon (`make exit-test` now asserts status 0).

### Items opened by this round

- **R1** — `setKeyGrab()` reinstalls all grabs for every binding: 5,260 `XGrabKey` requests at
  startup (~22 ungrab+regrab cycles of ~240 grabs). Batch them after the defaults are read.
- **R3** — `.gitignore` patterns `WindowMaker/menu*`/`plmenu*`/`appearance.menu*` match 95 tracked
  files; a future `menu.c`/`menu.h` would be silently ignored.
- **R5** — ~5.2 KB definitely lost at exit (WINGs `loadPixmaps()` pixmaps and the dock menu).
- **R7** — methodology: wrapper-based Valgrind runs say nothing about the WM; use
  `src/.libs/wmaker --for-real`.

### Commits produced by this verification

`151ccd71` (balloon teardown), `52e1029a` (defaults initialisation), `7b9f8e8e` (exit regression
test), `97b68f8a` (this document's saved answer) and the documentation update that follows them.

