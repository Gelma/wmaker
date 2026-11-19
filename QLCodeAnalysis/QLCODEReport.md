# CodeQL Analysis Report — Window Maker (gelma fork)

**Baseline scan:** 2026-11-19, CodeQL CLI 2.27.4, suite
`codeql/cpp-queries:codeql-suites/cpp-security-and-quality.qls` (183 queries),
run via `tools/codeql_analysis.sh` (isolated `/tmp` copy, `git clean -xdf`,
full feature configure, `CC=/usr/bin/gcc`, `CCACHE_DISABLE=1`).
**Artifacts:** `wmaker-results.csv` / `wmaker-results.sarif` (same directory)
hold the **post-remediation** scan; the 522 baseline is commit `36f08483`,
the post-P1 scan (512) is `a974dfd4`.
**Triage author:** Qwen Code, 2026-11-19. Supersedes the 2026-11-05/09 report
(563/53 and 447/19 eras). Remediation batches: SpecAI §169 (P1+P2) and §170
(P3+P4), both approved by Andrea; suppression policy **report-only**.

---

## 1. Baseline counts and coverage

**Baseline: 522 findings = 17 error / 168 warning / 337 recommendation**,
27 distinct rules with results. CSV and SARIF fully consistent (in SARIF the
severity lives in `rules[].defaultConfiguration.level`, `note` = CSV
`recommendation`). By component: src 198 · WINGs 123 · util 92 · WPrefs.app 72 ·
wrlib 35 · wmlib 2.

### Why 447 → 522 was coverage, not regression

The 2026-11-09 scan (`f1f9ee8f`, SpecAI §153) reported 447 and claimed "all
221 C translation units". A normalized diff shows **29 files with zero
findings in the old scan** that now produce findings: 18 WINGs widgets
(`wtext.c`, `selection.c`, `array.c`, `tree.c`, `wlist.c`, `wscroller.c`,
`wslider.c`, `wtabview.c`, …), 6 util dockapps (`wmbattery`, `wmclock`,
`wmcpugraph`, `wmeyes`, `wmmemgraph`, `wmnetgraph`), plus `util/xdnd.c`,
`wmlib/event.c`, `src/dockedapp.c`, `src/geomview.c`, `src/winspector.c`.
Files like `WINGs/wtext.c` cannot legitimately produce zero findings under
this suite: the old database never extracted them. Root cause (strong
inference; the old DB is gone): `database create --command=make` on an
already-built tree traces only recompiled translation units. The new script's
isolated clean build captures everything — **522 was the first trustworthy
full-coverage baseline**. Error trend: 53 (2026-11-05) → 19 (2026-11-09) →
17 (2026-11-19 baseline) → **13 today, all registered false positives**.

---

## 2. Error triage (17 baseline) — all verified against the code

### 2.1 Real bugs — FIXED (P1 batch, SpecAI §169)

| # | Rule | Site | Defect | Fix |
|---|------|------|--------|-----|
| 1 | cpp/inconsistent-null-check | `WINGs/dragsource.c:375` | `sourceScreen()` returns NULL once the source view is destroyed mid-drag; `sendEnterMessage` and the sink `sendDnDClientMessage:358` dereferenced unguarded (the :105 comment claiming they guarded was wrong); six other call sites were guarded | `ad949138` |
| 2 | cpp/inconsistent-null-check | `src/properties.c:54` | `XAllocClassHint()` unchecked → `XGetClassHint(dpy, win, NULL)` on OOM (Xlib mallocs, unlike `wmalloc` which aborts) | `57326aaf` |
| 3 | cpp/inconsistent-null-check | `src/window.c:688` | `classHint->res_name` dereferenced without NULL check in `fixLeaderProperties` | `ddf4f69a` |
| 4 | cpp/inconsistent-null-check | `src/window.c:761` | same in `createFakeWindowGroupLeader`; **bonus unflagged:** `XAllocWMHints()` fallback equally unchecked | `ddf4f69a` |

### 2.2 False positives / acceptable — registered in §7, no code action

**12 × cpp/unbounded-write** — every destination is `malloc/wmalloc(strlen(same
pieces) + slack)`; CodeQL cannot sum the literals. Verified site by site
(confirms the old §128 triage): userdefaults.c:77/108/110,
rootmenu.c:1309/1389/1425/1430/1432 (lines shift with edits), dialog.c:701,
getstyle.c:409, load.c:272/291.

**1 × cpp/inconsistent-null-check** — `src/event.c:911` (`configureMenu`):
NULL return (missing menu file) is tolerated by every consumer
(`if (scr->root_menu)`); effect is an empty root menu until the definition is
valid again — intended behavior, no deref.

---

## 3. Warning triage (168 baseline)

### cpp/path-injection — 62 — BY DESIGN (unchanged)
Paths from `HOME`/`WMAKER_USER_ROOT`/`GNUSTEP_CONFIG_FILE`/argv are the WM's
configuration mechanism; single-user trust model. New-code sites audited (P2):
telemetry.c (`open_private` = `open(O_NOFOLLOW, 0600)` + fchmod, dir from
`WM_RUNTIME_LOG_DIR`/`XDG_RUNTIME_DIR/wmaker`), shutdown.c (sentinel, §164),
dialog.c/dockedapp.c/winspector.c (user-config icon/pixmap loads). The
MouseSettings autostart flow moved from fopen to open in the P4 batch — same
by-design flow, count unchanged.

### cpp/missing-check-scanf — 11 — ALL FALSE POSITIVES (verified)
WPrefs.c ×7 (conjunctive `sscanf(A)!=3 && sscanf(B)!=3 → exit(1)` guard),
Focus.c:139 / HotCornerShortcuts.c:148 / TexturePanel.c:851 (`if (sscanf!=1)
i = default;`), appmenu.c:149 (outer sscanf guard `< 2 → return NULL`).

### cpp/integer-multiplication-cast-to-long — 20 baseline — FIXED (P3)
`becb538c` (wrlib ×10) + `b1e93444` (UI/geometry ×10): first operand widened
to `size_t`. The products were safe under the MAX_WIDTH/HEIGHT=20000 cap +
`RCreateImage` ckd_mul, but the invariant now lives at the use sites.
**Follow-up:** the post-cast range analysis surfaced one more site,
`RCombineImages`' bulk memcpy (raster.c), fixed in `62a6a430` — 21 casts total.
The same casts also closed 14 clang-tidy
`bugprone-implicit-widening-of-multiplication-result` findings.

### cpp/stack-address-escape — 6 — 4 FP / 1 mitigated / 1 by-design (audited, P2)
- proplist.c:1507/1572/1636 — FP: `PLData` is function-local (`wfree` before
  every return), the parser never retains it.
- util/xdnd.c:1593 — mitigated in-tree (`dnd.user_hook1 = NULL` at function
  end with explanatory comment); residual alert on the assignment itself.
- menuparser.c:79 — by design: parser local to `readMenu` (deleted before
  return), heap paths, include-parser uses `wstrdup`.
- handlers.c:142 — by design: all 59 timer call sites pass heap/NULL except
  menu.c `&d_data`, whose timer is confined to `menuMouseDown`'s frame
  (deleted on every exit path; one-shot auto-free verified in
  `W_CheckTimerHandlers`).

### cpp/toctou-race-condition — 3 — ACCEPTED (Coverity-triaged)
wfilepanel.c:716→730 (CID 1686069), wcolorpanel.c:3121→3129 (CID 1686074),
monitor.c:168→170 (CID 1686042, §164 rationale: unlink never follows symlinks,
the lstat is advisory). CodeQL does not read Coverity annotations.

### cpp/constant-comparison — 6 — FIXED (P1)
`b8f549ba` (actions.c ×4), `59f6822e` (KeyboardShortcuts chain_mode),
`652dc381` (switchmenu idx<0).

### cpp/world-writable-file-creation — 7 — 2 FIXED, 5 accepted
MouseSettings.c:655/670 **FIXED** (`11024f92`): autostart/autostart.tmp created
with `open(O_WRONLY|O_CREAT|O_TRUNC, 0700)` + fdopen — never world-writable
under any umask; the tmp file's final mode still set by the existing fchmod.
Remaining 5 (Appearance.c:467, wmiv.c:508, save_jpeg.c:185, save_png.c:199,
save_xpm.c:176): user's own files, 0666&~umask, accepted.

### cpp/uncontrolled-process-operation — 5 — BY DESIGN
main.c:239/560/587 (user-configured commands — the WM's purpose),
monitor.c:131 (own argv), WPrefs.c:631 (execlp of wmaker from PATH).

### Singles — FIXED (P4)
- wmclock.c:142 `localtime` → `localtime_r` + midnight fallback (`ad19b835`).
- FontSimple.c:524 double→int → explicit `(int)` cast (`257e4040`).

### cpp/poorly-documented-function — 46 — style, kept
The comment cleanup temporarily pushed 5 functions below the 2% ratio;
`92b5b277` documented them (purpose block + internal phase comments — the
query only counts comments inside the function extent, verified with a
minimal CodeQL DB). Count back to the baseline 46; bulk documentation pass
declined by decision.

---

## 4. Recommendation triage (337 baseline)

- **cpp/commented-out-code — 117 → 32**: selective cleanup (`8e544bcd`,
  `d18fba6c`, `e28452bb`, `2ef89d72`) removed 85 findings in 40 files.
  **Rule**: remove dead debug traces and superseded alternatives with no
  rationale; **keep** comments with explanations/TODO/rationale (the "-Dan"
  notes, menu.c inside_handler, appicon.c "can't remember why", proplist
  retain-count notes, wtext RESERVED/"never do this", widgets.c question),
  public-header API placeholders, the WMFreeArray memory-ownership questions,
  DND_DEBUG/XDND_VERSION toggles, the undo-button feature disable, and the
  TexturePanel test scaffolding. The 32 remaining are these kept comments.
- **cpp/non-constant-format — 1 — FIXED** (`ddb60271`): showError() takes a
  preformatted message; the caller-supplied-format sprintf is gone.
- Remaining families kept by decision (no bulk action): loop-variable-changed
  51 (idiomatic parsing), declaration-hides-* 61, long-switch 34, guarded-free
  24 (harmless; destructors NULL-safe per §155), trivial-switch 14,
  short-global-name 14, empty-block 8, fixme-comment 7, futile-conditional 5,
  complex-condition 1.

---

## 5. Remediation record

### P1 + P2 batch (SpecAI §169)
| Commit | Scope |
|--------|-------|
| `36f08483` | baseline csv/sarif (522, pre-fix) |
| `ad949138` | dragsource NULL guards + comment fix |
| `57326aaf` | PropGetWMClass XAllocClassHint check |
| `ddf4f69a` | window.c leader functions + XAllocWMHints bonus |
| `b8f549ba` `59f6822e` `652dc381` | 6 dead constant comparisons |
| `b39c3df7` | static-analysis re-baseline (cppcheck 1124→1121) |
| `a974dfd4` | post-P1 re-scan (512) |
| `e66d0a36` | this report (first full-coverage edition) |
| `3a905a4b` | SpecAI §169 |

P2 audits (menuparser/handlers callers, new-code path sites): no code changes.

### P3 + P4 batch (SpecAI §170)
| Commit | Scope |
|--------|-------|
| `becb538c` | wrlib size_t arithmetic (10 sites) |
| `b1e93444` | UI/geometry size_t arithmetic (10 sites) |
| `ddb60271` | showError preformatted message |
| `ad19b835` | wmclock localtime_r |
| `11024f92` | autostart owner-only creation |
| `257e4040` | FontSimple explicit cast |
| `8e544bcd` `d18fba6c` `e28452bb` `2ef89d72` | selective comment cleanup (85 findings, 40 files) |
| `008f91be` | static-analysis re-baseline (cppcheck 1121→1119, tidy 443→428) |
| `92b5b277` | document the 5 functions uncovered by the cleanup |
| `62a6a430` | RCombineImages size_t cast (surfaced by post-cast range analysis) |

### Verification evidence (both batches)
- gcc 14.2 full rebuild, project strict flags (`-Werror=trampolines`,
  `-std=gnu23`): **0 errors, 0 warnings** after each batch.
- clang-23 `-fsyntax-only` with exact build flags: clean on all touched files
  (6 in P1, 50 in P3/P4).
- `make check` **4/4**; `make exit-test` pass (SIGTERM/SIGUSR1/SIGUSR2);
  `EXIT_TEST_ARGS=--valgrind make exit-test` pass, no invalid accesses.
- Isolated Xvfb `:97` smoke (`/tmp/wm_smoke_p1.sh`, throw-away HOME, PID-only
  cleanup): **10/10** — switchmenu creation, root menu mapping, omnipresent
  ON/OFF + workspace round-trip (the edited ACTION_CHANGE_WORKSPACE branch,
  both directions), ShadeKey shade + titlebar-double-click unshade (both
  shade_animate branches), clean SIGTERM exit 0. Re-run after P3/P4.
- `tools/static-analysis.sh check`: no regressions; all count changes
  downward (see re-baseline commits).
- Not runtime-reproduced (static verification only): mid-drag source-view
  destruction (dragsource guard) and OOM paths (XAllocClassHint/XAllocWMHints,
  need fault injection).

---

## 6. Re-scan history (2026-11-19)

| Scan | Commit | Total | error | warning | rec | Delta |
|------|--------|-------|-------|---------|-----|-------|
| baseline (pre-fix) | `36f08483` | 522 | 17 | 168 | 337 | first full coverage |
| post-P1 | `a974dfd4` | 512 | 13 | 162 | 337 | −10 targets, 0 new |
| post-P3/P4 cleanup (intermediate) | — | 408 | 13 | 144 | 251 | −110, +6 (5 doc side effects + 1 site shift) |
| **final (post-doc + RCombineImages)** | committed with this report | **402** | **13** | **138** | **251** | −111 vs 512, +1 site shift (net zero) |

The 5 poorly-documented side effects were closed by `92b5b277`; the
MouseSettings path-injection "new" finding is the same by-design autostart
flow moved from fopen to open (net zero). The RCombineImages multiplication
surfaced only after the other casts widened the analyzer's dimension ranges
(fixed in `62a6a430`). **The 13 residual errors are exactly the registered
FP set (§7); no real error remains open.**

---

## 7. Report-only suppression registry (approved policy)

No in-source `// codeql[...]` annotations are added; this registry is the
authoritative record of accepted findings. Re-scans keep reporting them;
consult this table before re-triaging. Line numbers shift with edits — match
by rule + file + pattern.

| Family | Sites | Verdict / rationale |
|--------|-------|---------------------|
| cpp/unbounded-write | userdefaults.c ×3, rootmenu.c ×5, dialog.c:701, getstyle.c:409, load.c ×2 | FP: destination sized from strlen of the same pieces (§2.2) |
| cpp/inconsistent-null-check | event.c:911 (configureMenu) | FP: NULL tolerated by all consumers (§2.2) |
| cpp/missing-check-scanf | WPrefs.c ×7, Focus.c, HotCornerShortcuts.c, TexturePanel.c, appmenu.c | FP: guards verified (§3) |
| cpp/stack-address-escape | proplist.c ×3 | FP: PLData function-local (§3) |
| cpp/stack-address-escape | xdnd.c:1593 | mitigated in-tree, residual alert (§3) |
| cpp/stack-address-escape | menuparser.c:79, handlers.c:142 | by design, caller audit (§3) |
| cpp/toctou-race-condition | wfilepanel.c, wcolorpanel.c, monitor.c | accepted; Coverity CIDs 1686069/1686074/1686042 (§3) |
| cpp/path-injection | 62 sites | by design: user-config trust model; telemetry/shutdown hardened (§3) |
| cpp/uncontrolled-process-operation | main.c ×3, monitor.c, WPrefs.c | by design: WM executes user commands (§3) |
| cpp/world-writable-file-creation | Appearance.c:467, wmiv.c:508, save_jpeg/png/xpm | umask-dependent, user's own files (§3) |
| cpp/poorly-documented-function | 46 large functions | style; bulk documentation pass declined (§3) |
| cpp/commented-out-code | 32 kept comments | intentional: rationale/TODO/API/memory-question comments (§4) |
| style families | loop-variable-changed, hides-*, long/trivial-switch, guarded-free, short-global, empty-block, fixme, futile, complex-condition | kept by decision (§4) |

## 8. Open items

**None from the CodeQL plan** — every actionable finding from the §169 triage
is closed (P1–P4 all implemented and re-scanned). What remains in the reports
is the registered FP/by-design set above plus the style families kept by
decision. Future re-scans should be diffed against this registry before any
new triage round.