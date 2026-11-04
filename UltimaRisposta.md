# Round-2 review of the fix batch `d0dd9291..HEAD` (saved answer, 2026-11-04)

> This file preserves, in English, the answer given after reviewing the batch of commits that
> implemented the fixes for the 19 findings of `DeepSeekCheck.md`.
> Repository: `../gelma_github_repo` (the commit `d0dd9291` only exists there); the code tree is
> identical to `wmaker_git` HEAD `2f65b5e3`, apart from `README`.
> Nothing was modified during the review: the temporary test patch was reverted and `git status`
> was clean.

The commit `d0dd9291` does not exist in `wmaker_git`: it was found in `../gelma_github_repo`
(commit "Readme update"), where `d0dd9291..HEAD` contains exactly the fixes for the previous 19
findings. The code tree is identical to `wmaker_git` HEAD `2f65b5e3` (only `README` differs), so
the review and the runtime verification were carried out there.

## How it was verified

Full `clang-23` rebuild, Valgrind **on the real binary**, isolated Xvfb sessions, gdb, XTest,
`wm_stress`, runtime telemetry.

Important methodological correction: `valgrind ./src/wmaker` (the libtool wrapper) **does not
trace the WM** after the `exec` chain; use `valgrind src/.libs/wmaker --for-real` (or
`--trace-children=yes`). This is why neither the previous review nor this batch caught the problem
below.

## 🔴 Critical problem: every shutdown ends in SIGSEGV

It is not caused by these fixes, but it makes most of them ineffective:

```
Invalid read of size 8 at address 0x8
  WMSetBalloonTextForView (wballoon.c:93)   <- scr->balloon == NULL
  destroyView (wview.c:332)
  W_DestroyBalloon (wballoon.c:477)
  WMDestroyScreen (widgets.c:1195)
  wScreenDestroy (screen.c:1673)
  Shutdown (shutdown.c:100)  <- SIGTERM
```

Cause: `d7035384` (previous range) changed

```c
W_DestroyBalloon(scr->balloon);  scr->balloon = NULL;   /* before: worked */
```
into
```c
balloon = scr->balloon;  scr->balloon = NULL;  W_DestroyBalloon(balloon);  /* now crashes */
```

because `destroyView()` always calls `WMSetBalloonTextForView(NULL, view)`, which dereferences
`view->screen->balloon->table`.

Evidence: `SIGTERM` and `SIGUSR1` → **exit status 139** (both before and after the fixes); gdb
backtrace and Valgrind trace identical. Applying, temporarily, the guard
`if (!view || !view->screen || !view->screen->balloon) return;` makes the exit return **status 0**.

Current consequences: `Exit(0)`, `ExecExitScript()` and `wCleanupSignalPipe()` never run; a core
dump is produced on every exit; the monitor reports nothing because the exit sentinel exists.
Therefore fix 1 (self-pipe) is correct but **cannot yet guarantee a clean exit**, and fix 10 is
**unreachable** (the crash happens before the font loop in `WMDestroyScreen`).

## Assessment of each fix

| # | Fix | Verdict | Evidence |
|---|---|---|---|
| 1 | self-pipe | ✅ correct, ⚠️ goal not achieved | SIGUSR2 → dump (`uptime 13.39s`) and clients intact; SIGTERM/USR1 handled; handler only `write()` + `errno`; CLOEXEC/NONBLOCK fds; idempotent cleanup. But it ends in SIGSEGV (above). Leftover: `SIG_WCHANGE_STATE` macro now dead |
| 2 | de-advertise sync | ✅ functional, ⚠️ incomplete | `_NET_SUPPORTED` without `_NET_WM_SYNC_REQUEST*`; ~40 lines of dead code remain: empty stubs `wNETWMUpdateSyncCounter`/`wNETWMHandleSyncAlarmNotify`, `sync_request` in `window.h`, atoms, `XSyncQueryExtension`, alarm teardown in `window.c:302-311`, protocol bit never set |
| 3 | ScreenCapture | ✅ correct | end-to-end test with a 240-char template → 244-char file, WM alive, no overflow. Nit: truncation of the final `snprintf` is not detected |
| 4 | key-grab dedup | ✅ correct | exact 32-bit scan, `grab_seen` removed; `key_grabs_deduplicated` stays 0 (no real duplicates) |
| 5 | double rebind | ⚠️ partial | `rebindKeygrabs()` removed correctly, but `setKeyGrab()` reinstalls **all** grabs for every binding: measured **5,260 XGrabKey at startup** (~22 ungrab+regrab cycles of ~240). Should be batched at the end of the read sequence |
| 6 | XSync metric | ✅ correct | 478 instead of 15,386 in a comparable run |
| 7 | JSON keys | ✅ correct | no duplicate keys; `ExtensionOrOther_85` unique |
| 9 | KSM/HugeTLB | ✅ half solved | `MAP_HUGETLB` removed; single `prctl` in `WMInitializeApplication()`. **KSM still on by default** (the side-channel risk was not addressed) and per-buffer `MADV_MERGEABLE` is redundant with the prctl |
| 10 | font cache | ✅ code correct, ⚠️ not verifiable | snapshot into `WMArray` + `WMPopFromArray` is right, but the balloon crash happens first: today that code never runs |
| 13 | warnings | ✅ correct | 10 → **6 warnings** (the 4 new ones gone); `wraster.h` regenerated without `nonnull`. Nit: `(void)mode_bit;` is a band-aid; removing `__wrlib_nonnull(3)` changes the public API contract |
| 15 | duplicated helpers | ✅ correct | `wWindowIsManaged()` and `wScreenSetKeyGrabsSuspended()` used everywhere; `wScreenSetKeyGrabs()` still does not consult the suspension flag (harmless today: only the unsuspend path calls it) |
| 16 | wm_stress | ✅ correct | `_Atomic`, divide-by-zero clamp, `memset` on hints; 6 s stress ok. Not done: hard-coded `-lpthread` (use `$(PTHREAD_LIBS)`), no `TESTS` in `make check` |
| 17 | .gitignore | ⚠️ works but imprecise | `git status` clean, but the patterns are broad: **95 tracked files** match `WindowMaker/menu*`/`plmenu*`; a future `menu.c`/`menu.h` would be silently ignored. Better: `WindowMaker/menu.[a-z][a-z]*` |
| 19 | MapRequest | ⚠️ does not address the issue | the `scr` guard is harmless but covers a different case: there is still no log/counter for the `wWindowFor() != NULL` branch and for `wManageWindow() == NULL`, i.e. for the real anomaly |

## Other observations collected while verifying

- **New (minor)**: with the guard applied, Valgrind on the WM reports
  `Conditional jump depends on uninitialised value` in `wDefaultsInitDomain()` (startup) —
  pre-existing.
- **Remaining leaks at exit** (again with the guard): ~5.2 KB `definitely lost`, mostly WINGs
  pixmaps created by `loadPixmaps()`; therefore the "0 definitely lost" claim does not hold for
  the WM itself.
- **`SpecAI.md` is self-contradictory**: the *Verification Status* section still claims
  `_NET_WM_SYNC_REQUEST` "fully implemented, tested, and verified" and "Direct Huge Pages
  implemented and verified", the opposite of what these commits just did; the *Review Backlog —
  2026-11-03* section still describes the pre-fix state (sync advertised, signals unsafe).
- **No regression tests were added**: every fix is manual; the balloon crash would have been
  caught by an `xvfb-run + SIGTERM + --error-exitcode=1` test on the real binary.
- Unaddressed findings remain: **8** (paths in `/tmp`, removed from the document as "temporary"),
  **11** (context in `wApplicationDestroy`), **12** (L1 cache), **18** (`wmaker-valgrind-session`
  default tool), plus the security half of **9** and the observability half of **19**.

## Bottom line

The fixes are, on the whole, **correct and well targeted**: 1, 3, 4, 6, 7, 13, 15, 16 are verified
working; 2, 5, 9, 17, 19 are functional but incomplete or imprecise; 10 is correct but not yet
reachable. The problem to put first is the **SIGSEGV on every shutdown**, which nullifies the
value of fix 1, hides fix 10 and invalidates the clean-exit claims.
