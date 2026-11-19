# CodeQL Analysis Report — Window Maker (gelma fork)

**Baseline scan:** 2026-11-19, CodeQL CLI 2.27.4, suite
`codeql/cpp-queries:codeql-suites/cpp-security-and-quality.qls` (183 queries),
run via `tools/codeql_analysis.sh` (isolated `/tmp` copy, `git clean -xdf`,
full feature configure, `CC=/usr/bin/gcc`, `CCACHE_DISABLE=1`).
**Artifacts:** `wmaker-results.csv` / `wmaker-results.sarif` (same directory).
**Triage author:** Qwen Code, 2026-11-19. Supersedes the 2026-11-05/09 report
(563/53 and 447/19 eras), which was removed from the tree.

---

## 1. Baseline counts and coverage

**522 findings = 17 error / 168 warning / 337 recommendation**, 27 distinct
rules with results. CSV and SARIF are fully consistent (same 522 results,
same rule→severity map; in SARIF the severity lives in
`rules[].defaultConfiguration.level`, `note` = CSV `recommendation`).

By component: src 198 · WINGs 123 · util 92 · WPrefs.app 72 · wrlib 35 · wmlib 2.

### Why 447 → 522 is coverage, not regression

The 2026-11-09 scan (commit `f1f9ee8f`, SpecAI §153) reported 447 findings and
claimed "all 221 C translation units". A normalized diff (line numbers stripped
from messages) shows **29 files with zero findings in the old scan** that now
produce findings: 18 WINGs widgets (`wtext.c`, `selection.c`, `array.c`,
`tree.c`, `wlist.c`, `wscroller.c`, `wslider.c`, `wtabview.c`, …), 6 util
dockapps (`wmbattery`, `wmclock`, `wmcpugraph`, `wmeyes`, `wmmemgraph`,
`wmnetgraph`), plus `util/xdnd.c`, `wmlib/event.c`, `src/dockedapp.c`,
`src/geomview.c`, `src/winspector.c`. Files like `WINGs/wtext.c` (3000+ lines
of upstream code full of commented-out blocks) cannot legitimately produce
zero findings under this suite: the old database simply never extracted them.

Root cause (strong inference; the old DB no longer exists): `codeql database
create --command=make` on an already-built tree only traces translation units
that make actually recompiles. The new script's isolated clean build
(`git clean -xdf` + autogen + full configure + full make under the tracer,
ccache disabled) captures everything. **522 is the first trustworthy
full-coverage baseline.** The error trend is still downward:
53 (2026-11-05) → 19 (2026-11-09) → 17 (2026-11-19), and of today's 17 only
4 were real bugs (§2), now fixed (§5).

The diff also confirms the §154/§155 fixes dropped out: 2 float-`sprintf`
errors, 4 nullness errors, the `wfilepanel.c` redundant null check,
`load_jxl.c` 2 multiplication overflows + TOCTOU, `switchmenu.c`
empty-if/futile conditional.

---

## 2. Error triage (17) — all verified against the code

### 2.1 Real bugs — fixed in this batch (§5)

| # | Rule | Site | Defect | Fix |
|---|------|------|--------|-----|
| 1 | cpp/inconsistent-null-check | `WINGs/dragsource.c:375` (`sendEnterMessage`) | `sourceScreen()` returns NULL once the source view is destroyed mid-drag (`endedDrag` may destroy it); `sendEnterMessage` and the common sink `sendDnDClientMessage:358` dereferenced unguarded. The comment at :105 claiming those paths "already guard" was wrong. Six other call sites were guarded. | `ad949138` — guard both (return False), guard the XDND_DEBUG trace site, correct the comment |
| 2 | cpp/inconsistent-null-check | `src/properties.c:54` (`PropGetWMClass`) | `XAllocClassHint()` unchecked → `XGetClassHint(dpy, win, NULL)` on OOM. Xlib allocates with malloc (unlike `wmalloc`, which aborts). | `57326aaf` — NULL check, fall back to the "default" class/instance already used for unreadable properties |
| 3 | cpp/inconsistent-null-check | `src/window.c:688` (`fixLeaderProperties`) | `classHint->res_name` dereferenced without NULL check on OOM | `ddf4f69a` — skip class-hint propagation on OOM, keep group/command/notify fixes; guard final XFree |
| 4 | cpp/inconsistent-null-check | `src/window.c:761` (`createFakeWindowGroupLeader`) | `classHint->res_name = instance` without NULL check; **bonus, unflagged:** `XAllocWMHints()` fallback in the same function equally unchecked | `ddf4f69a` — guard both; leader window still created, hints degraded on OOM |

The statistical nullness rule surfaced these because the §154 fixes raised the
checked fraction of `XAllocClassHint`/`sourceScreen` call sites above threshold.

### 2.2 False positives / acceptable — no code action (registry in §7)

**12 × cpp/unbounded-write** — every destination is `malloc/wmalloc(strlen(same
pieces) + slack)`; CodeQL cannot sum the literals. Verified site by site
(confirms the §128 triage; lines only shifted):

- `WINGs/userdefaults.c:77` — `wmalloc(pathlen + sizeof(subdir))`
- `WINGs/userdefaults.c:108/110` — `slen = strlen(gspath)+strlen("/" DEFAULTS_SUBDIR "/")+strlen(domain)+1`
- `src/rootmenu.c:1309/1389/1425/1430/1432` — `malloc(strlen(path)+strlen(name)+4/+6+…)`
- `src/dialog.c:701` — `wmalloc(strlen(tmp)+strlen(iconFile)+4)`
- `util/getstyle.c:409` — `wmalloc(strlen(ThemePath)+32)` for `"/style"` (7 bytes)
- `wrlib/load.c:272/291` — `malloc(strlen(file)+1)`

**1 × cpp/inconsistent-null-check** — `src/event.c:911` (`configureMenu`):
returns NULL when the menu file is missing; `scr->root_menu = NULL` is
tolerated by every consumer (e.g. `WA_OPEN_APPMENU` checks `if (scr->root_menu)`).
Effect of NULL: empty root menu until the definition is valid again — intended
behavior, no deref. Optional future nicety: a wwarning on failed rebuild.

---

## 3. Warning triage (168)

### cpp/path-injection — 62 — BY DESIGN
Paths derived from `HOME`/`WMAKER_USER_ROOT`/`GNUSTEP_CONFIG_FILE`/argv are the
WM's configuration mechanism; single-user trust model. Distribution: defaults.c 6,
userdefaults.c 4, dialog.c 4, wmiv.c 4, icon/screen/telemetry/wmsetbg 3 each, rest ≤2.
New-code sites reviewed in this batch (P2 audit):
- `src/telemetry.c:451/680/761` — `open_private()` = `open(O_NOFOLLOW, 0600)` +
  `fchmod(0600)`; dir = `WM_RUNTIME_LOG_DIR` (explicit user override) or
  `XDG_RUNTIME_DIR/wmaker` (private per-user) or `TELEMETRY_DIR`. Hardened and
  documented in-source. Accepted.
- `src/shutdown.c:76` — exit sentinel path; hardened per SpecAI §164
  (`O_EXCL|O_NOFOLLOW`, unlink never follows symlinks). Accepted.
- `src/dialog.c:443/706/782/1732`, `src/dockedapp.c:107`, `src/winspector.c:492` —
  icon/pixmap loads from the user's own config tree via `wfindfile`. Accepted.

### cpp/missing-check-scanf — 11 — ALL FALSE POSITIVES (verified)
- `WPrefs.app/WPrefs.c:703/705/710/714` (×7): guard
  `sscanf(A)!=3 && sscanf(B)!=3 → exit(1)`; continuing implies at least one call
  wrote all three out-params. The query does not model the conjunctive guard.
- `Focus.c:139`, `HotCornerShortcuts.c:148`, `TexturePanel.c:851`:
  `if (sscanf(...) != 1) i = default;` — initialization guaranteed.
- `src/appmenu.c:149`: `code` written by the outer sscanf (line 93, guard `< 2 →
  return NULL`); `etag` by the `!= 4`/`!= 5` guards (123/133). Query loses the
  flow across the loop.

### cpp/integer-multiplication-cast-to-long — 20 — SAFE TODAY, FRAGILE INVARIANT (P3, pending approval)
wrlib sites (`raster.c:219/271/675`, `convert.c:351/352/361/362`, `convolve.c:48/58`,
`gradient.c:139/341`): image dimensions capped at `MAX_WIDTH=MAX_HEIGHT=20000`
(raster.c:85-86) and `RCreateImage` uses `ckd_mul/ckd_add`; worst product
20000²×4 = 1.6×10⁹ < INT_MAX, so the `int` multiplications at the memcpy sites
cannot overflow **today**. The invariant lives in the constants, not at the use
sites: raising the cap above ~23170/side would make them real. Cheap `(size_t)`
casts recommended (P3, not yet approved). src/util sites (dock, properties,
screen, xinerama ×2, wmagnify, wcolorpanel, Appearance ×2): UI geometry, same
cheap cast.

### cpp/stack-address-escape — 6 — 4 FP / 1 mitigated / 1 by-design (audited)
- `WINGs/proplist.c:1507/1572/1636` — **FP verified**: `PLData` is heap but
  function-local (`wfree(pldata)` before every return); the parser only reads
  `pldata->ptr/pos`, never retains it. The `desc/file/command` parameters cannot
  outlive the call.
- `util/xdnd.c:1593` — **already mitigated in-tree**: `dnd.user_hook1 = NULL`
  at function end with an explanatory comment citing this very query. The alert
  persists because the query flags the assignment itself. Residual, harmless.
- `WINGs/menuparser.c:79` — **by design (P2 audit)**: the only external caller
  is `readMenu()` (rootmenu.c:1074) with a heap path; the parser is
  `WMenuParserDelete`d before `readMenu` returns; the include-parser path uses
  `wstrdup(req_filename)` (menuparser.c:573) and propagates the parent's
  `DEF_CONFIG_PATHS` literal. Stored pointers never outlive the frame.
- `WINGs/handlers.c:142` — **by design (P2 audit)**: all 59 in-tree
  `WMAddTimerHandler`/`WMAddPersistentTimerHandler` call sites pass heap widget
  pointers or NULL, except `src/menu.c:2038/2207` (`&d_data`, stack). There the
  timer lifetime is confined to `menuMouseDown`'s frame: it can only fire from
  that function's own event loop, `delaySelection` nulls `magic` on fire,
  one-shot handlers are freed/unlinked after firing
  (`W_CheckTimerHandlers`, handlers.c:326-338), and **every** exit path deletes
  a pending timer (normal exit menu.c:2270, `byebye:` menu.c:2337).

### cpp/toctou-race-condition — 3 — ACCEPTED (already triaged for Coverity)
- `WINGs/wfilepanel.c:716→730` — stat → confirm alert → remove; in-source
  `coverity[toctou] CID 1686069` ("delete dialog on the user own tree").
- `WINGs/wcolorpanel.c:3121→3129` — access → overwrite alert → remove;
  `coverity[toctou] CID 1686074`.
- `src/monitor.c:168→170` — lstat(sentinel)+uid → unlink, post-`febaf042`
  (CID 1686042). §164 rationale: `unlink` never follows symlinks; the check is
  advisory (decides only whether to break the restart loop).
CodeQL does not read Coverity annotations, hence the residual alerts.

### cpp/constant-comparison — 6 — FIXED (§5)
`actions.c:1363/1368/1374` (steps≡16), `actions.c:2454` (s clamped ≥1),
`KeyboardShortcuts.c:398` (timeout_ms clamped ≥1, chain_mode never reassigned),
`switchmenu.c:354` (idx≡−1 at check). Dead guards/variables removed, behavior
unchanged.

### cpp/world-writable-file-creation — 7 — LOW RISK (P4 optional)
`fopen(...,"w")` creates with 0666&~umask: world-writable only under umask 0.
`MouseSettings.c:655/670` (autostart): `fchmod(fileno(fo), permissions)` at :701
sets the final mode explicitly; residual window on the tmp file in the user's
own directory. `Appearance.c:467`, `wmiv.c:508`, `save_jpeg.c:185`,
`save_png.c:199`, `save_xpm.c:176`: user's own files. Optional hardening:
`open(O_CREAT|O_WRONLY, 0700)` + `fdopen` for the autostart script.

### cpp/uncontrolled-process-operation — 5 — BY DESIGN
`main.c:239/560/587` (exec/system of user-configured commands — the WM's
purpose; the real command-injection cases were closed in the §128 batch),
`monitor.c:131` (execvp of its own argv), `WPrefs.c:631` (execlp of wmaker
from PATH). Accepted.

### Singles
- `util/wmclock.c:142` `localtime` — single-threaded dockapp, by design;
  `localtime_r` is a 2-line optional fix (P4).
- `WPrefs.app/FontSimple.c:524` double→int lossy cast — cosmetic, verify
  intent (P4).
- `cpp/poorly-documented-function` 46 — style; no action (bulk documentation
  pass declined, §4).

---

## 4. Recommendation triage (337) — no bulk action (approved decision)

Almost all style/maintainability, largely surfaced by the newly covered files
(upstream heritage): commented-out-code 117, loop-variable-changed 51
(idiomatic parsing in dockapps), declaration-hides-* 61, long-switch 34,
guarded-free 24 (harmless; destructors are NULL-safe per §155), trivial-switch 14,
short-global-name 14, empty-block 8, fixme-comment 7, futile-conditional 5,
complex-condition 1. **Decision: skip bulk cleanup** (high churn on upstream
files, low value); selective removal only where clearly dead, on request.

The one security-adjacent recommendation:
- `cpp/non-constant-format` — `WINGs/wfilepanel.c:601` `showError()`:
  `sprintf(errStr, s, file)` with `s` a translated literal from the 5 callers
  (3 pass file=NULL, 2 pass `floppyPath` with a single `%s`). Buffer sized
  `strlen(s)+strlen(file)+1` → safe today; a translation adding a `%s` would be
  UB. Optional refactor to a preformatted message (P3, pending approval).

---

## 5. Remediation record — 2026-11-19 batch (P1 + dead comparisons)

| Commit | Scope |
|--------|-------|
| `36f08483` | baseline csv/sarif committed (522, pre-fix) |
| `ad949138` | dragsource.c: NULL guards in sendDnDClientMessage/sendEnterMessage (+XDND_DEBUG site), comment corrected |
| `57326aaf` | properties.c: XAllocClassHint NULL check in PropGetWMClass |
| `ddf4f69a` | window.c: XAllocClassHint NULL checks in fixLeaderProperties + createFakeWindowGroupLeader, XAllocWMHints bonus |
| `b8f549ba` | actions.c: 4 dead constant-comparison guards removed |
| `59f6822e` | KeyboardShortcuts.c: always-true chain_mode removed |
| `652dc381` | switchmenu.c: dead `idx < 0` conjunct removed |

P2 audits (menuparser/handlers callers, new-code path-injection sites) produced
**no additional code changes** — verdicts recorded in §3.

### Verification evidence
- Full rebuild (gcc 14.2, project strict flags incl. `-Werror=trampolines`,
  `-std=gnu23`): **0 errors, 0 warnings**; the 6 touched files recompiled
  explicitly and checked.
- clang-23 `-fsyntax-only` with the exact build flags on all 6 touched files:
  **clean**.
- `make check`: **4/4 PASS**.
- `make exit-test`: **3/3 PASS** (SIGTERM exit 0, SIGUSR1 restart, SIGUSR2
  reload+telemetry).
- `EXIT_TEST_ARGS=--valgrind make exit-test`: **4/4 PASS**, no invalid memory
  accesses during shutdown.
- Isolated Xvfb smoke (`:97`, throw-away HOME, PID-only cleanup,
  `/tmp/wm_smoke_p1.sh`): **10/10 PASS** — WM registration, xterm management,
  window-list menu mapping (switchmenu creation), root menu mapping,
  omnipresent ON/OFF with workspace round-trip (exercises
  `UpdateSwitchMenu(ACTION_CHANGE_WORKSPACE)` in both directions — the edited
  switchmenu branch), ShadeKey shade (client unmaps) and titlebar-double-click
  unshade (client remaps) — exercises both `shade_animate` branches including
  the edited UNSHADE loop — workspace switch, clean SIGTERM exit 0.
- `tools/static-analysis.sh check`: cppcheck **1124 → 1121** (style −3, the
  removed dead code), error 6 / warning 2 / portability 10 unchanged;
  clang-tidy **443 unchanged**; no regressions. Baseline re-written and
  committed (`b39c3df7`).
- Not reproduced at runtime: the mid-drag source-view-destruction scenario
  (dragsource guard — statically verified, matches the six guarded siblings)
  and the OOM paths (XAllocClassHint/XAllocWMHints — require fault injection).
  The WPrefs `capture_shortcut` change is semantically identical (variable
  always True) and covered by build + clang check.

---

## 6. Post-fix re-scan (2026-11-19)

Re-run of `tools/codeql_analysis.sh` (same suite/toolchain, isolated clean
build) after the six fix commits, committed alongside this report:

**512 findings = 13 error / 162 warning / 337 recommendation** (522 → 512, −10).

- **Closed: exactly the 10 targeted findings, zero new findings** (normalized
  diff baseline → post-fix):
  - 4 real errors: dragsource `sourceScreen` nullness (`ad949138`),
    properties.c `XAllocClassHint` (`57326aaf`), window.c `XAllocClassHint` ×2
    (`ddf4f69a`);
  - 6 `cpp/constant-comparison` warnings: actions.c ×4 (`b8f549ba`),
    KeyboardShortcuts.c (`59f6822e`), switchmenu.c (`652dc381`).
- The statistical nullness rule produced **no churn**: raising the checked
  fraction did not surface new sites.
- **Residual 13 errors = exactly the registered false-positive set** (§7):
  12 unbounded-write (strlen-sized allocations) + event.c:911 configureMenu
  (NULL tolerated). No real error remains open.
- Residual warnings of note: 20 multiplication casts (safe under the current
  cap, P3 casts pending approval), 62 path-injection (by design), 11 scanf
  (FP), 46 poorly-documented (style).

---

## 7. Report-only suppression registry (approved policy)

Andrea chose **report-only** suppression: no in-source `// codeql[...]`
annotations are added; this registry is the authoritative record of accepted
findings. Re-scans will keep reporting them; consult this table before
re-triaging.

| Family | Sites | Verdict / rationale |
|--------|-------|---------------------|
| cpp/unbounded-write | userdefaults.c:77/108/110, rootmenu.c:1309/1389/1425/1430/1432, dialog.c:701, getstyle.c:409, load.c:272/291 | FP: destination sized from strlen of the same pieces (§2.2) |
| cpp/inconsistent-null-check | event.c:911 (configureMenu) | FP: NULL tolerated by all consumers (§2.2) |
| cpp/missing-check-scanf | WPrefs.c ×7, Focus.c:139, HotCornerShortcuts.c:148, TexturePanel.c:851, appmenu.c:149 | FP: guards verified (§3) |
| cpp/stack-address-escape | proplist.c:1507/1572/1636 | FP: PLData function-local (§3) |
| cpp/stack-address-escape | xdnd.c:1593 | mitigated in-tree (user_hook1=NULL), residual alert (§3) |
| cpp/stack-address-escape | menuparser.c:79, handlers.c:142 | by design, caller audit (§3) |
| cpp/toctou-race-condition | wfilepanel.c:730, wcolorpanel.c:3129, monitor.c:170 | accepted; Coverity CIDs 1686069/1686074/1686042, §164 rationale (§3) |
| cpp/path-injection | 62 sites (§3 list) | by design: user-config trust model; telemetry/shutdown hardened (§3) |
| cpp/uncontrolled-process-operation | main.c:239/560/587, monitor.c:131, WPrefs.c:631 | by design: WM executes user commands (§3) |
| cpp/world-writable-file-creation | MouseSettings.c:655/670, Appearance.c:467, wmiv.c:508, save_jpeg.c:185, save_png.c:199, save_xpm.c:176 | umask-dependent, user's own files; autostart fchmod'd (§3) |
| cpp/potentially-dangerous-function | wmclock.c:142 (localtime) | single-threaded dockapp (§3) |
| style families | 337 recommendations (§4) | no bulk action by decision |

## 8. Open items (awaiting explicit approval)

- **P3 remainder:** `(size_t)` casts for the 20 multiplication sites (fragile
  cap invariant), `showError` refactor (non-constant format).
- **P4 optional:** `localtime_r` (wmclock), explicit 0700 for autostart
  creation, FontSimple lossy cast, selective commented-out-code cleanup.
- Optional diagnosability: wwarning when the root-menu rebuild returns NULL
  (event.c:911 path).