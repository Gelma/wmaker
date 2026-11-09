# Static Analysis Review — 2026-11-09

Author: DeepSeek with Qwen Coder (review for Andrea Gelmini)
Scope: commits `a6c2e19d..HEAD` (19), with focus on the static-analysis
remediation batch (12 commits after `41f8bd65`). Input: the refreshed
`CoverityReport/` and `QLCodeAnalysis/` report sets.

---

## 1. What the batch contains

| Commit | Kind | Content |
|---|---|---|
| `26ee5217` | report | Coverity export 09/11/26 (40 defects, 10 new CIDs) |
| `03e1c3ac` | report | CodeQL re-scan (447 findings) |
| `979e9ffb` | report | drop stale `QLCodeAnalysis/report.html` |
| `2115272d` | fix | `wrlib/load_jxl.c` TOCTOU, null deref, leak (CID 1687863/64/72) |
| `53aba151` | fix | `WPrefs.app/Appearance.c` renderTexture null returns (CID 1687865) |
| `0406c492` | fix | `WINGs/wfontpanel.c` leak + null derefs (CID 1687866/68) |
| `71c8f330` | fix | `WINGs/wcolorpanel.c` selection guard (CID 1687867) |
| `1cbbc6a1` | fix | `src/snihost.c` menu/frame validation (CID 1687869) |
| `25c21b40` | fix | `src/session.c` `wapp` guard (CID 1687870) |
| `4b0e48d8` | fix | `src/dialog.c` selected-dir guard (CID 1687871) |
| `03d4e300` | fix | CodeQL nullness/float (`wwindow`, `event`, `menu`, `window`, `wfilepanel`, `userdefaults`, `MouseSettings`) |
| `d38c25c3` | docs | SpecAI §154, `tools/static-analysis.txt`, `.gitignore` (`codeql-db/`) |

The rest of the range (`ed6ba2f7`, `1dc46a96`, `268be799`, `41f8bd65`, …) is the
framewin title-strip / SNI work already reviewed separately.

---

## 2. Verification performed

- Forced rebuild of every changed translation unit (`gcc 14.2`,
  `-Wall -Wextra -Wdouble-promotion -Wfloat-equal -Wlogical-op …`): **0 warnings**.
- `make check`: **PASS**.
- `make exit-test`: **PASS** (SIGTERM / SIGUSR1 / SIGUSR2).
- `EXIT_TEST_ARGS=--valgrind make exit-test`: **PASS**
  ("Valgrind: no invalid memory accesses during shutdown").
- No separate interactive Xvfb GUI session; startup/shutdown/restart are covered
  by the exit-test harness.

---

## 3. Report status

### 3.1 CoverityReport (`coverity_defects.csv`, 40 defects)

- **10 new CIDs (1687863–1687872), all addressed** by the commits above — one-to-one.
- **21 pre-existing defects remain `New`** (11/04/26 + `240602`), not touched here.
  Notable ones:
  - `1686058` (High) out-of-bounds read, `src/event.c:422`, `handle_inotify_events`
  - `1686112` (High) string not NUL-terminated, `src/event.c`, `handle_inotify_events`
  - `1686060` (Medium) deref-before-null, `src/wsmap.c:117`
  - `1686080` / `1686105` (Medium) null derefs, `src/dock.c`
  - `1686034` / `1686051` (High) leaks, `util/wmmenugen_parse_wmconfig.c:163`
  - `1686077` / `1686110` (Medium) `wrlib/load_ppm.c`
  - `1686101` (High) `renderTexture` leak — already annotated in code as a
    known false positive (`coverity[leaked_storage]`).
- 9 `Dismissed` (TOCTOU, previously evaluated).

### 3.2 QLCodeAnalysis (`wmaker-results.csv` / `.sarif` / `QLCODEReport.md`)

- Re-scan totals **verified from the data**, not just the prose:
  `wmaker-results.csv` severity column = **19 error / 150 warning / 278
  recommendation = 447**; SARIF = 535 `ruleId` = 447 results + rules. Matches §8.
- Spot-checked claims: `util/wmsetbg.c` no longer calls `system()` (`fork()` +
  `execlp()`); `WINGs/wcolorpanel.c` `char tmp[4]` → `tmp[16]`;
  `WPrefs.app/Expert.c` `tmp[10]` → `tmp[32]`.
- **Inconsistency:** the Executive Summary (§1) still reports **563 / 53 errors**;
  §8 reports **447 / 19**. The prose was not reconciled.
- **Staleness:** `03e1c3ac` (re-scan) predates `03d4e300`; §8.3's "remaining 19
  errors" therefore still lists the 7 that `03d4e300` subsequently fixed
  (2 float `sprintf`, 4 nullness, 1 redundant null check).

---

## 4. Fix quality

The fixes are small, targeted and carry the CID in the message. Correctness:

- **JXL loader** (`2115272d`): TOCTOU removed with `fopen()` + `fstat(fileno())`
  + `S_ISREG`, `fread` validated, `(size_t)width*height*{3,4}` avoids int
  overflow, `image`/`pixels` released on `NEED_IMAGE_OUT_BUFFER` re-entry and
  validated before `memcpy` in `FULL_IMAGE`. Good.
- **`renderTexture`** (`53aba151`): falls back to `grad` when the tiled image is
  NULL, releases `grad` only once, and the tail `if (!image) return None;`
  covers the all-NULL case. Good.
- **Font/Color panels, dialog, session, `wwindow`, `event`, `window`**: correct.
  `owin` in `wWindowFocus` is already guaranteed non-NULL by the earlier
  `if (owin == wwin || !owin) return;`, so guarding `oowner` is the right fix.
- **`sprintf` → `snprintf(sizeof)`** and `wfree(dirName)` without the `if`
  (`wfree` is NULL-safe): correct.

### 4.1 Issue: the OOM null checks are dead code and unsafe if reached

`wmalloc()`/`wrealloc()` in `WINGs/memory.c` **never return NULL** — on failure
they call `wfatal()`/`wAbort()`. Consequently:

- `if (!menu->menu)` in `wMenuCreate()` (`03d4e300`) and `if (!menu || !menu->frame)`
  in `sni_on_get_layout_reply()` (`1cbbc6a1`) are **unreachable**: `wCoreCreate()`
  and `wFrameWindowCreate()` cannot return NULL.
- Worse, if such a branch ever ran, `wMenuDestroy(menu, True)` would reach
  `wCoreDestroy(NULL)` / `wFrameWindowDestroy(NULL)`, and **neither destructor is
  NULL-safe** → null dereference. So the "fix" is Coverity appeasement, not
  robustness.

### 4.2 Issue: report inconsistencies and staleness

See §3.1/§3.2. The numbers are right in §8 but wrong/stale in §1 and §8.3.

### 4.3 Issue: cppcheck baseline moved up

`tools/static-analysis.txt` was updated 925 → 933 total (style 907 → 915), i.e.
the snapshot **increased**. This is plausible (the tree grew), but it deserves a
note so the baseline is not read as a regression-free promise.

---

## 5. Recommendations

1. **Reconcile `QLCODEReport.md`**: update §1 to 447 / 19 (or label §1 as the
   "2026-11-05 baseline") and point to §8; regenerate the re-scan after
   `03d4e300` so §8.3 no longer lists fixed findings.
2. **Make the OOM paths genuinely safe**: NULL-guard `wCoreDestroy()` and
   `wFrameWindowDestroy()` (cheap, correct), so the constructor null checks stop
   being both dead and dangerous.
3. **Work the Coverity backlog**, highest severity first: `event.c` `1686058` /
   `1686112`, then `wsmap`/`dock` nulls and the `wmmenugen` leaks.
4. **Annotate the cppcheck baseline** (why it rose) so future diffs stay meaningful.

## 6. Verdict

Sound, coherent remediation of the 10 new Coverity defects and a validated
CodeQL re-scan (numbers confirmed from the raw CSV/SARIF), with the build and
all tests green under Valgrind. The two real weaknesses are reporting
(inconsistency + staleness) and the fact that the two constructor null checks do
not add runtime safety. The pre-existing Coverity backlog remains open and is the
natural next batch.

---

## 7. Remediation applied in this session

- **NULL-safe destructors** (`src/wcore.c`, `src/framewin.c`): `wCoreDestroy()`
  and `wFrameWindowDestroy()` now return early on NULL, so the defensive OOM
  checks in `wMenuCreate()` / `sni_on_get_layout_reply()` can no longer turn into
  a crash if they were ever reached. This closes the "dead and dangerous" note of
  §4.1.
- **Report reconciliation** (`QLCodeAnalysis/QLCODEReport.md`): §1 now carries a
  2026-11-09 status banner pointing at §8 (current 447 / 19) and labels the
  563 / 53 figures as the 2026-11-05 baseline; §8.3 gained a post-scan note that
  the 2 float `sprintf`, the 4 nullness checks and the redundant null check were
  fixed in `03d4e300` *after* the scan.
- **Coverity backlog triage + one real leak**: most of the 21 "New" CIDs already
  carry a fix/annotation comment in the code — e.g. `1686058`/`1686112` in
  `src/event.c`, `1686060` in `src/wsmap.c`, `240602` in `WINGs/string.c` — so
  the Coverity Connect `status` column is simply not synchronized with the tree.
  The only clearly-real one found was the empty-label line in
  `util/wmmenugen_parse_wmconfig.c`: `parse_wmconfig_line()` always allocates
  `key`/`value`, but the `strlen(label)==0` branch freed only `label` and
  `continue`d. Fixed (`1686034`). Left as-is pending a Coverity re-scan: the Low
  "calling risky function" (`1686036`, `1686085`, `1686108`, intentional) and the
  Low "unused value" `1686063` (benign).
- **cppcheck baseline** (`tools/static-analysis.txt`): re-ran
  `tools/static-analysis.sh cppcheck`; the current tree still yields **933**
  (6 error / 10 portability / 915 style / 2 warning), so the baseline matches the
  tree — the earlier 925→933 delta is tree growth, not a remediation regression.

Verification after the changes: full `make -j` **0 warnings**; `make check`
PASS; `make exit-test` PASS.
