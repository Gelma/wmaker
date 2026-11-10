# Coverity Remediation Review — 2026-11-10

Author: DeepSeek with Qwen Coder (review for Andrea Gelmini)
Scope: commits `632f84ca` (Coverity report refresh), `0bf8c849`, `e0da4354`, `dbf27dab`,
`f7e191f9`, `880a6887` (fixes) and `c8a41b10` (SpecAI §158).

---

## 1. Report state

`CoverityReport/coverity_defects.csv` was refreshed (36 defects, down from 40): **27 `New`,
9 `Dismissed`**. The refresh introduced 9 new CIDs (`1687878`–`1687889`). The five fix commits
address **22 of the 27 `New`** CIDs.

Remaining `New` entries and why they are not fixed:
- `1686081` (dock `killCallback`) — already annotated in code as a false positive (`WCHECK_STATE` then
  `WCHANGE_STATE` idiom).
- `1686110` (`load_ppm.c` `pm_getuint` loop) — already annotated in code as a false positive.
- `1686036`, `1686085`, `1686108` — Low "calling risky function", intentional.

The CSV `status` column is a **snapshot taken before the fixes**, so it still lists the
just-fixed CIDs as `New`. A re-scan is what will close them; no further code change is needed
for the remaining entries.

---

## 2. Verification

- Forced rebuild of all 16 touched translation units (gcc 14.2, strict flags): **0 warnings**.
- `make check`: **PASS**.
- `make exit-test`: **PASS** (SIGTERM / SIGUSR1 / SIGUSR2).
- `EXIT_TEST_ARGS=--valgrind make exit-test`: **PASS** ("no invalid memory accesses during shutdown").

---

## 3. Fix quality

- **`0bf8c849` (wrlib)** — `load_jxl.c`: the removed `image = NULL` was truly redundant (the very next
  statement reassigns `image`). `load_webp.c` / `load_ppm.c`: `fread(dst, 1, n)` + exact byte count is
  strictly better than `fread(dst, n, 1)`; it now also catches partial reads. Correct.
- **`e0da4354` (WINGs)** — `wfont.c` guards `scrPtr`/`fontName`; `wfilepanel.c` clamps the list
  position to `>= 0`; `wscrollview.c` guards the scroller pointers in addition to the flags. Correct.
- **`dbf27dab` (wm)** — `session.c`: the `assert(dc != NULL)` is replaced by a full guard and `name`
  is initialized to `NULL`, so `if (name != NULL)` is safe. `event.c`: the inotify filename is copied
  into a bounded 256-byte buffer with `min(pevent->len, 255)` and NUL-terminated — fixes both the
  out-of-bounds read and the non-terminated string. `switchpanel.c`: `panel->tileTmp` guard.
  `wsmap.c`: the `!wsmap->win` check is now **before** `workspace_map_unslide()` (the previous order
  still dereferenced the null window) — a real fix. `defaults.c`: the removed `dict = NULL` is dead
  (the variable is not read again before the next domain reassigns it).
- **`f7e191f9` (dock)** — `execCommand` frees the token array on the `argc == 0` path (`wfree(NULL)` is
  safe); `openDockMenu`/`iconDblClick`/`drawerRestoreState` add the missing null/type guards
  (`iconDblClick` also turns a release-build `assert` into a real return). Correct.
- **`880a6887` (util/WPrefs)** — `Configurations.c` guards the loaded image; `wmmemgraph.c` checks
  every `sscanf`; `setstyle.c` checks `XParseColor` and falls back to the input color string;
  `--ignore` is reworked to a distinct option character (removes the dead `case 0` flag dance);
  `wmsetbg.c` removes a provably dead guard (`image` is checked non-NULL right after `loadImage`).
  The `getColor` fallback `value = c1 ? c1 : c2` does **not** break refcounting: the caller builds a
  `WMCreatePLArray(..., value, ...)` which retains it before `WMReleasePropList(value)`.

---

## 4. Notes / remaining

- **Only a Coverity re-scan remains** to synchronize the `status` column; the tree already contains
  the fixes/annotations for every reported defect.
- Minor, pre-existing (not introduced here): the first `getColor()` caller in `setstyle.c` does not
  release `value` when `getColor` returns a freshly created string, so that string leaks. Trivial for
  a one-shot CLI tool; noted for completeness.
