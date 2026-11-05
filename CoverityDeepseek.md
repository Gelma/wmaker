# Coverity report - triage and corrective plan

Static-analysis export committed under `CoverityReport/`: `coverity_defects.csv` and
`coverity_defects.json` (the 88-row table), plus one `cid_<n>.json` per defect with the checker name
and the full event path.

| item | value |
|---|---|
| report | Coverity, 88 defects |
| tree | `gelma` branch, as of 2026-11-04 |
| files triaged | all 90 files in `CoverityReport/` |
| plan key | the `cid` (Coverity id); line numbers drift, ids do not |

## 1. What the report contains

88 defects: **30 High**, **44 Medium**, **14 Low**. 83 were first seen in the 2026-11-04 run;
5 are old ones (2013-2014) that were still open.

One more arrived on **2026-11-05** as `cid_1686136.json` alone (no CSV row): a resource leak in
`src/main.c` introduced by the guard added for CID 1686037, fixed the same day - see §8.

| area | defects |
|---|---|
| `src/` (WM core) | 37 |
| `WINGs/` (widget toolkit) | 23 |
| `util/` (wmgenmenu, wmsetbg, wmiv, xdnd, setstyle) | 15 |
| `WPrefs.app/` | 9 |
| `wrlib/` | 3 |
| `wmlib/` | 1 |

By type, the mass is: resource leaks 17, TOCTOU 9, dereference-after-null-check 7, dereference of a
null return 7, integer overflow 6, unchecked return 5, operands that do not affect the result 4,
dereference before null check 4, logically dead code 4, use after free 3, and a long tail of single
findings (out-of-bounds read, string not NUL terminated, copy-paste, uninitialized pointer,
untrusted loop bound, ...).

## 2. Method

Every `cid_*.json` was read (checker, occurrence paths, main event), cross-checked against the CSV,
and then **the reported line and its surroundings were read in the current tree**. The report is
from the same day, so nothing had been fixed in between, but Coverity reasons about the code
abstractly: a defect is only real if the semantics of the surrounding functions allow it.

Each defect got one of five verdicts:

- **fix** - a real defect with a local, low-risk change;
- **verify+fix** - real shape, needs a Valgrind/sanitizer run to pin the exact path;
- **harden** - cannot happen with well-formed input (kernel events, own files, X server replies),
  but the check is cheap and removes a whole class;
- **hygiene** - dead code or an unused value that hides intent, no behavioural bug;
- **false positive / accepted** - suppressed, with the reason recorded.

Reading the code also turned up five problems that Coverity did not report: two are the *root
cause* of reported symptoms, one is a real heap-overflow risk behind a mis-filed integer report,
and two are latent bugs found while judging a false positive. They are in §3.

## 3. Root causes worth fixing once

### 3.1 `wtokensplit()` leaves `*argv` untouched for an empty command

`WINGs/string.c:95` only assigns `*argv` inside the token loop. A command that tokenises to nothing
leaves the caller's pointer as it was: uninitialised on the first call, **stale and already freed**
on any later call in the same frame. That is the real defect behind:

- cid 1686037 (`util/wmgenmenu.c:413`, "using freed pointer argv"), where `argv` is declared once
  outside the loop and freed at the end of each iteration;
- cid 240602 (`src/dock.c:3172`, `argv` leaking out of scope).

Action: initialise `*argv = NULL; *argc = 0;` at the top of `wtokensplit()`, then audit the four
callers (`src/dock.c:226`, `src/dock.c:3165`, `src/main.c:379`, `src/session.c:352`) for
`argv[0]`/`argv[argc-1]` uses that assume `argc > 0`.

### 3.2 `WMCreatePLArray()` retains, it does not consume

`WINGs/proplist.c:964` calls `WMRetainPropList()` on every element, so the inline idiom

```c
prop = WMCreatePLArray(WMCreatePLString("spixmap"),
                       WMCreatePLString(panel->imageFile), NULL);
```

leaks one reference per string. The codebase shows the correct pattern a few hundred lines away
(`WMAddToPLArray(array, obj); WMReleasePropList(obj);`), and Coverity flags the inline form at
cid 1686094, 1686100 and 1686109 - it would flag many more with a better model (§6).

Action: add a small helper to WINGs (`WMAddNewPLString(array, str)` or similar), use it at the
flagged sites, and document the ownership rule next to `WMCreatePLArray()`'s prototype.

### 3.3 `WMEraseFromBag()` does not renumber the following nodes

Found while checking cid 1686055 (itself a false positive: `rbTreeDelete()` updates `tree->root`,
so `self->root` is never the freed node). `WINGs/bagtree.c:451` deletes the node but, unlike
`treeDeleteNode()` at line 423, does not decrement the `index` of every successor. After one
`WMEraseFromBag(i)` the bag's indices are stale, so a later lookup by index can miss or hit the
wrong element.

Action: make `WMEraseFromBag()` use `treeDeleteNode()`, or add the same successor-renumbering loop;
add a WINGs bag unit test that erases from the middle and then reads by index.

### 3.4 `RCreateImage()` results used unchecked

`RCreateImage()` returns NULL on OOM (and above the 20000-pixel dimension cap). Four sites
dereference the result immediately: cid 1686040 (`WPrefs.app/Appearance.c:591`), 1686053
(`WINGs/wcolorpanel.c:2808`), 1686078 (`src/texture.c:365`), 1686079 (`src/defaults.c:3602`).
Same class, so one pass: check the return and degrade (skip the texture, keep the panel up).

### 3.5 `wmalloc()` does not zero, and `__wmessage()` appends into it

`WINGs/memory.c:89` is a plain `malloc` (plus the huge-page advice); it never zeroes. `__wmessage()`
then starts with `strncat(buf, progname, linemax - 1)` on that fresh buffer: `strncat()` first walks
`buf` looking for a NUL, so it reads uninitialised memory, and the `linemax - 1 - strlen(buf)`
computed right after can underflow into an unbounded append. Coverity filed this as a mere integer
overflow (cid 1686071); it is a heap-overflow risk with an uninitialised read in front of it, which
is why it belongs in batch 1 and not in "hardening".

Action: `buf[0] = '\0'` right after the allocation, then track the remaining space as `size_t`
instead of recomputing `linemax - strlen(buf)`.

## 4. Plan

Order is chosen by damage, not by count: memory safety first, then arithmetic that can corrupt
pixels or geometry, then leaks in code that runs once, then hardening, then cosmetics. Batches 1-4
are independent of each other and are committed separately; batch 5 is optional.

| batch | defects | shape of the change | risk |
|---|---|---|---|
| 1 - crash safety | 16 | NULL guards, save-before-free, `wtokensplit()`, bag renumbering, `__wmessage` | low, but touches `WINGs/wtext.c`, `WINGs/error.c`, `src/icon.c`, `src/menu.c` |
| 2 - integer arithmetic | 7 | signed offsets, clamps, one cast | very low, no behaviour change intended |
| 3 - leaks | 17 | free what is overwritten, release PL strings, free on error paths | low |
| 4 - checked returns and bounds | 14 | check Xlib/libc results, bound the inotify loop | low |
| 5 - hygiene and intent | 15 | remove dead code, make the modifier macro honest | very low, optional |
| suppressed | 20 | `/* coverity[...] */` comments + model | none |

### Batch 1 - crash safety (16 + 2 root causes)

`WINGs/wtext.c` (use after free in `deleteTextInteractively`: `tb->graphic` is read after
`WMDestroyTextBlock()` freed `tb` - save the flag first; and the two `cursorToTextPosition` paths
that reach `tb->...` after `tb` was nulled), `WINGs/error.c` (§3.5), `src/icon.c:663`
(`icon->icon_win` with `icon == NULL`), `src/dock.c:3600` (`aicon->icon->selected`),
`src/switchpanel.c:188`, `src/menu.c:1226`, `src/winspector.c:1210`, `src/workspace.c:950`,
`src/dock.c:1675`, the four `RCreateImage()` sites (§3.4), plus `wtokensplit()` (§3.1) and
`WMEraseFromBag()` (§3.3).

The `src/dock.c:1675` case is the one to do carefully: it is the same function this tree already
fixed for a NULL dock at shutdown (SpecAI §125), so the new guard has to stay consistent with it.

Verification: full build (0 warnings), `make exit-test`, `EXIT_TEST_ARGS=--valgrind make exit-test`,
the out-of-tree `-fsanitize=address,undefined` build running the same gate, the three `wrlib` tests,
plus a WPrefs/panel smoke run on a private Xvfb for the two WPrefs-adjacent items. `__wmessage` gets
a direct check: a message long enough to fill `linemax` must not corrupt the heap (Valgrind run).

### Batch 2 - integer arithmetic (7)

All seven are "compute it signed" or "clamp it": `wsplitview.c:363/484` (`(int)_GetSplitViewSize() -
getTotalSize()`), `dragsource.c:736` (`(int)nchildren - 1`), `wtextfield.c:199/426/1247` (clamp
`viewPosition`/`cursorPosition` to `[0, textLen]` before indexing and before `textLen - vp`), and
`WPrefs.app/Appearance.c:2015` (cast to `double` before the integer division that feeds `sqrt()`).

Verification: the UI is the test - switch panels, drag a divider, type in a text field with UTF-8 at
the edges, open the Appearance panel - plus the standard gates. These changes must not alter
rendering: the exit test and a screenshot comparison on Xvfb are enough.

### Batch 3 - leaks (16)

Three groups:

1. the menu generators (`util/wmmenugen_parse_wmconfig.c`, `util/wmmenugen_parse_xdg.c`,
   `util/wmgenmenu.c`), where structures and strings are overwritten or simply abandoned at the end
   of the parse;
2. WPrefs panels (`Appearance.c`, `TexturePanel.c`, `HotCornerShortcuts.c`) with the same pattern,
   plus the PL-array ownership fix (§3.2);
3. two small ones in the WM (`src/misc.c:910` on the error path, `src/window.c:1007` overwriting
   `class`), `wmlib/menu.c:57` (`entryline2` also on the failure path) and `src/screen.c:751`
   (`randr_state` on the abort path).

Verification: `valgrind --leak-check=full --show-leak-kinds=definite` on `wmgenmenu` over a small
menu tree, before and after, and on WPrefs on a private Xvfb; record the totals in the commit
message. Widget leak checks are noisy (Xlib, Pango), so compare only `definitely lost`.

### Batch 4 - checked returns and bounds (14)

Xlib results (`XQueryTree`, `XGrabPointer`, `XParseColor`), libc results (`poll`, `fcntl`, `mkdir`,
`rename`, `remove`), one NULL-dictionary guard, one NULL `typelist` guard, and the inotify loop
bound (`i + sizeof(struct inotify_event) + pevent->len <= eventQLength`). The inotify pair is not a
live bug - the kernel delivers complete events - but it turns a "trust me" into a check, and it is
the only way to silence cid 1686058/1686112 without a suppression.

Verification: build + gates; for the poll/fcntl paths, exercise SIGTERM/SIGUSR1/SIGUSR2 (the exit
test) and a session with a slow selection owner for `getTextSelection`.

### Batch 5 - hygiene and intent (15)

Dead code and unused values, plus the `store_modifier` macro in `src/xmodifier.c`: the four
`modifier_index == x_bit && *(old_mod) != x_bit` tests are trivially false when `old_mod` is the
address of that same variable. Behaviour is correct (a warning is not wanted in that case), but the
expression reads like a bug and hides that the warning is only meant for *cross* assignments; say
so explicitly (`old_mod != &x_bit`) and the four "operands don't affect result" reports disappear
on their own.

Verification: `tools/static-analysis.sh check` (the count baseline should drop), build + gates.
Do this last, in one commit, only for the items that do not change behaviour.

## 5. What is suppressed, and why

The 20 suppressed items fall into three groups:

- **TOCTOU (7)**: `access()`/`stat()` before use in WPrefs, the palette panel, the file panel,
  `setstyle`, `wmmenugen`, `proplist.c` and `monitor.c`. All of them operate on files owned by the
  same user, in the same trust domain; replacing check-then-use with the atomic syscall changes the
  UI flow (the dialogs report "already exists" before asking) for no security gain. Record the
  reasoning, do not change the code.
- **weak crypto (3)**: `rand()` in `DoKaboom`, `randomPlaceWindow` and the resize animation.
  Nothing cryptographic depends on it. If a blanket rule is wanted, the fix is a random *source*
  change (`lrand48`/`arc4random`), not a defect fix.
- **false positives (10)**: `fread()` checks that look at the item count (1686030, 1686077), the
  scrollview "flag cleared before the call" pair (1686067, 1686106), the state-machine idiom
  (1686081), the defensive `break` (1686049), the tree-root update in `rbTreeDelete()` (1686055),
  the tainted PPM loop (1686110), and the two popup/palette paths.

For each of them: a `/* coverity[checker_name] */` comment on the reported line with the reason, so
the next export shows them as intentional rather than as a growing backlog.

## 6. Coverity model (so the next run is quieter and sharper)

`tools/coverity-model.c` is the delivered file (load it as a modeling file in the analysis
settings; it is not part of the build). After writing it, the honest expectation is **small**: the
checker already infers allocation from the `w…alloc`/`w…dup` names, and every leak it reported in
this export was real - the code was fixed instead of teaching it otherwise. What the model does is
make four statements that are literally true in the tree:

```c
void *wmalloc(unsigned long size);              /* fresh block of `size`     */
char *wstrdup(const char *s);                   /* fresh copy, unknown size  */
char *wstrndup(const char *s, unsigned long n); /* fresh block of at most n+1 */
void  wfree(void *p);                           /* releases p, NULL accepted */
```

The value is precision, not suppression: the same obligations at every call site instead of the few
the heuristics happened to understand. Measure it - one analysis with the file, one without, compare
the per-checker counts; anything that appears *only* with the model is a modelling error.

What the file deliberately leaves out, because a "more complete" model would be worse than none:

| left out | why |
|---|---|
| `wrealloc(p, n)` | a model can say "frees p and allocates" (invents a use-after-free when realloc fails and keeps the block) or "allocates, keeps p" (invents a leak on every success) |
| `WMReleasePropList()`, `RReleaseImage()`, `WMDestroyWidget()` | reference counted: they free only at zero, so modelling them as free produces a use-after-free for every released-but-still-referenced object |
| `WMCreatePLArray()`, `WMCreatePLString()` factories | the interesting property is that they *retain*; a model cannot express that without also hiding the real leak (§3.2), which was fixed in the code |
| `Exit()`, `wAbort()`, `EventLoop()` | already declared `noreturn` in the headers |

Two things are therefore not delegated to the model: `RCreateImage()` may return NULL (the callers
check it, §3.4) and the `WMBag` index semantics behind §3.3 are a code invariant, not an API
contract a model could express.

## 7. Definition of done

1. Batches 1-4 committed separately, each with its verification in the commit message.
2. Every suppressed cid carries its `coverity[...]` comment or a model entry, with the reason in
   one line.
3. `make exit-test`, its `--valgrind` variant, the sanitizer build of §125 (SpecAI) and the three
   `wrlib` tests stay green after each batch; `tools/static-analysis.sh check` is re-baselined only
   when the count legitimately drops.
4. The next Coverity export shows zero open High defects, and the diff of `coverity_defects.csv`
   against this one is explainable item by item.

## 8. Implementation status (2026-11-05)

Batches 1 to 5 are implemented: one commit per CID, hashes in the appendix, and the
twenty suppressed items carry a `/* coverity[...] */` comment with the reason instead of a
change. Several fixes came with a proof: `test/wings_bag_test` fails on the old
`WMEraseFromBag` (bag[3] reads back 0), and the XPM test fails on the old loader (the
transparent entry came out as 190,190,190).

Verification after the last commit: full build **0 errors / 0 warnings**; `make exit-test`
and `EXIT_TEST_ARGS=--valgrind make exit-test` pass; `wrlib_blend_test`, `wings_bag_test`
and `wrlib_xpm_test` pass; `tools/static-analysis.sh check` is green against a
re-baselined count (cppcheck 930 -> 926).

Two verdicts changed while implementing, and both are recorded in the code:

| cid | planned | actual |
|---|---|---|
| 1686101 | verify, then fix | **false positive**: the reported path needs a texture type that is `...pixmap` and `solid` at the same time. Annotated in place. |
| 1686086 | remove the dead branch | the `MAX_OPTIONS` guard **is reachable** with 129 `--ignore` options; deleting it would let `ignoreList` overflow, so it is suppressed instead. |

Three things the report did not have, found while implementing:

- `WMMenuCreate()` freed the wrong buffer on a partial allocation (CID 1686066): the test
  freed `entryline` when it was NULL and leaked `entryline2`.
- `parse_wmconfig()` never cleared `Exec`, so an entry without an `exec` line inherited the
  previous command; the same helper now clears it.
- The `WMCreatePLArray(WMCreatePLString(...))` idiom was the norm in `GetTexturePanelTexture()`
  (15 sites) and `wmgenmenu` (2 sites), not just at the reported lines: helper functions now
  build the array and release the temporaries.

CID 1686055 no longer exists as reported: `WMEraseFromBag()` goes through `treeDeleteNode()`,
so there is no post-delete access to test.

A ninth leak (CID 1686136) arrived on 2026-11-05, after this plan was closed, and is part of the
same work: the `argc == 0` guard added for CID 1686037 returned without freeing `command`, the
`wtokenjoin()` buffer that the function owns. Coverity reported `argv` (NULL by construction);
the real leak was the string. Both are freed now, without relying on the invariant.

## 9. Appendix - item by item

Every cid of the export, grouped by batch. "sev" and "type" are Coverity's, "location" is
`file:line` at the time of the triage (the cid is the stable key).

| bucket | count |
|---|---|
| Batch 1 - crash safety | 16 |
| Batch 2 - integer arithmetic | 7 |
| Batch 3 - leaks | 16 |
| Batch 4 - checked returns and bounds | 14 |
| Batch 5 - hygiene and intent | 15 |
| No change - suppressed | 20 |
| **total** | **88** |

### Batch 1 - crash safety (16)

| cid | sev | type | location | action | commit |
|---|---|---|---|---|---|
| 1686096 | Medium | Dereference after null check | `src/winspector.c`:Various (createInspectorForWindow) | **fix** - Guard `panel->appFrm`: the inspector is created before the page widgets exist on this path. `7a316140` |
| 1686091 | Medium | Dereference after null check | `src/menu.c`:1226 (keyboardMenu) | **fix** - Check `menu` for NULL at entry: wMenuUnmap() dereferences it via the cascade list. `cfbf87aa` |
| 1686084 | Medium | Dereference null return value | `src/workspace.c`:950 (wWorkspaceRestoreState) | **fix** - Guard the WMGetFromPLString() result before wstrndup: a malformed WMState entry reaches it. `7f58d234` |
| 1686080 | Medium | Dereference after null check | `src/dock.c`:Various (openDockMenu) | **fix** - Guard `aicon->icon` (clip and drawer icons can have none) before reading ->selected. `a4e5f41d` |
| 1686079 | Medium | Dereference null return value | `src/defaults.c`:3602 (chopOffImage) | **fix** - Return NULL from chopOffImage() when RCreateImage() fails; the caller already handles NULL. `1c5d3870` |
| 1686078 | Medium | Dereference null return value | `src/texture.c`:365 (wTextureRenderImage) | **fix** - Check the RCreateImage() result before RClearImage in the WTEX_SOLID branch. `5e8e7974` |
| 1686072 | High | Read from pointer after free | `WINGs/wtext.c`:Various (deleteTextInteractively) | **fix** - Save `was_graphic = tb->graphic` before WMDestroyTextBlock() frees tb, then test the saved value. `8872274a` |
| 1686071 | High | Overflowed integer argument | `WINGs/error.c`:Various (__wmessage) | **fix** - Initialise the buffer (`buf[0] = '\0'`): strncat() on the freshly wmalloc'd, non-zeroed buffer computes strlen over garbage, and the following `linemax - strlen(buf)` can then underflow into an unbounded append. Then track the remaining space as size_t. `f79dc500` |
| 1686070 | Medium | Dereference after null check | `WINGs/wtext.c`:Various (cursorToTextPosition) | **fix** - Same function as 1686032: the `goto _doNothing` path reaches `tb->nsections` after tb was nulled. `a1119770` |
| 1686054 | Medium | Dereference after null check | `src/switchpanel.c`:188 (changeImage) | **fix** - Guard `icon == NULL` before WMSetFrameRelief: changeImage runs while the icon may still be unset. `02c8d43e` |
| 1686053 | Medium | Dereference null return value | `WINGs/wcolorpanel.c`:2808 (customRenderSpectrum) | **fix** - Check `spectrum` for NULL before `spectrum->data` (RCreateImage fails on OOM). `32f3fa22` |
| 1686040 | Medium | Dereference null return value | `WPrefs.app/Appearance.c`:591 (renderTexture) | **fix** - Check the RCreateImage() result before RClearImage, in every branch of renderTexture. `e73864d1` |
| 1686037 | High | Read from pointer after free | `util/wmgenmenu.c`:413 (find_and_write) | **fix** - Same root cause as 240602: wtokensplit() leaves *argv untouched for an empty command, so argv keeps the pointer freed by the previous iteration. `87fcba49` |
| 1686033 | Medium | Dereference after null check | `src/icon.c`:663 (wIconUpdate) | **fix** - Return early when `icon == NULL`; the `icon->icon_win` branch assumes a live icon. `3fe5df25` |
| 1686032 | Medium | Dereference before null check | `WINGs/wtext.c`:1194 (cursorToTextPosition) | **fix** - Move the `tb` NULL test before its first use: the `_doNothing` label is reachable with tb == NULL. `3af87be1` |
| 240602 | High | Resource leak | `src/dock.c`:3172 (execCommand) | **fix** - Root cause in wtokensplit(): it must set *argv = NULL, *argc = 0 up front; then the argc==0 path here has nothing to leak. `87fcba49` |

### Batch 2 - integer arithmetic (7)

| cid | sev | type | location | action | commit |
|---|---|---|---|---|---|
| 1686087 | Medium | Result is not floating-point | `WPrefs.app/Appearance.c`:2015 (createPanel) | **fix** - Cast before dividing: `sqrt((double)wlengthof(sample_colors) * available_width / available_height)`. `0b68d2d4` |
| 1686082 | High | Overflowed integer argument | `WINGs/wtextfield.c`:426 (WMDeleteTextFieldRange) | **fix** - Same clamp as 1686057, applied in WMDeleteTextFieldRange. `7cc448aa` |
| 1686059 | High | Overflowed integer argument | `WINGs/wtextfield.c`:Various (handleTextFieldKeyPress) | **fix** - Clamp `viewPosition`/`cursorPosition` to [0, textLen] before paintTextField uses them as indices. `e57a8cab` |
| 1686057 | High | Overflowed integer argument | `WINGs/wtextfield.c`:Various (decrToFit) | **fix** - Clamp `vp` to [0, textLen] before `text[vp]`, and compute the length in int, not `textLen - vp`. `94649cec` |
| 1686050 | Medium | Overflowed constant | `WINGs/dragsource.c`:736 (findDestination) | **fix** - `i = (int)nchildren - 1`: with unsigned nchildren the 0 case is an implementation-defined conversion, even if it lands on -1 here. `c29261bc` |
| 1686038 | High | Overflowed integer argument | `WINGs/wsplitview.c`:363 (handleViewResized) | **fix** - Same as 1686031, in handleViewResized. `9551b85a` |
| 1686031 | High | Overflowed integer argument | `WINGs/wsplitview.c`:484 (dragDivider) | **fix** - Make the offset signed: `(int)_GetSplitViewSize() - getTotalSize(sPtr)`. The unsigned subtraction wraps when contents are larger than the view, and the conversion back to int is implementation-defined. `c474a605` |

### Batch 3 - leaks (17)

| cid | sev | type | location | action | commit |
|---|---|---|---|---|---|
| 1686136 | n/a | Resource leak | `src/main.c`:384 (RelaunchWindow) | **fix** - free `command` and the (empty) token array on the `argc == 0` return; the guard added for CID 1686037 leaked the command string. | `d7e12fed` |
| 1686109 | High | Resource leak | `WPrefs.app/TexturePanel.c`:Various (GetTexturePanelTexture) | **fix** - Same pattern as 1686094, in GetTexturePanelTexture(). `fd77e85e` |
| 1686104 | High | Resource leak | `src/screen.c`:751 (wScreenInit) | **fix** - Free scr->randr_state on the abort_no_context path. `cb518d4f` |
| 1686101 | verify, then fix | **false positive**: the reported path needs a texture type that is `...pixmap` and `solid` at the same time. Annotated in place. |
| 1686100 | High | Resource leak | `util/wmgenmenu.c`:Various (find_and_write) | **fix** - Same pattern as 1686094, for the L3Menu/L2Menu construction. `9039154e` |
| 1686098 | High | Resource leak | `WPrefs.app/Appearance.c`:2361 (OpenExtractPanelFor) | **fix** - Keep the epanel pointer (or free it) instead of letting it go out of scope. `dcbb7d69` |
| 1686094 | High | Resource leak | `util/wmgenmenu.c`:Various (other_window_managers) | **fix** - WMCreatePLArray() retains its arguments: keep each WMCreatePLString in a variable and WMReleasePropList() it after the array is built. `66f737e6` |
| 1686075 | High | Resource leak | `src/window.c`:1007 (wManageWindow) | **fix** - Free the previous `class` (from PropGetWMClass) before replacing it with strdup. `b7ff7166` |
| 1686068 | High | Resource leak | `WPrefs.app/HotCornerShortcuts.c`:Various (storeData) | **fix** - Do not overwrite `str` with the literal "None": use a `const char *out` for the value while still freeing the WMGetTextFieldText() copy. `c791c179` |
| 1686066 | High | Resource leak | `wmlib/menu.c`:57 (WMMenuCreate) | **fix** - Free entryline2 too when entryline allocation fails (wmlib/menu.c). `311fcb74` |
| 1686056 | High | Resource leak | `util/wmmenugen_parse_wmconfig.c`:142 (parse_wmconfig) | **fix** - Free the wmc structure at the end of parse_wmconfig. `07e8278a` |
| 1686052 | High | Resource leak | `util/wmmenugen_parse_xdg.c`:196 (parse_xdg) | **fix** - Free the wm entry at the end of parse_xdg. `44c95857` |
| 1686051 | High | Resource leak | `util/wmmenugen_parse_wmconfig.c`:Various (parse_wmconfig) | **fix** - Free the previous value before overwriting wmc->Name/Exec/Category/Restart, and free the leftovers at exit. `d3b7d035` |
| 1686048 | High | Resource leak | `src/misc.c`:910 (GetShortcutString) | **fix** - wfree(tmp) on the `mod < 0` error path in GetShortcutString(). `abf2f212` |
| 1686043 | High | Resource leak | `util/wmmenugen_parse_wmconfig.c`:142 (parse_wmconfig) | **fix** - Free the wm entry at the end of parse_wmconfig. `a074e6c4` |
| 1686039 | High | Resource leak | `util/wmmenugen_parse_xdg.c`:Various (parse_xdg) | **fix** - Free the xdg entry at the end of parse_xdg, including the path where no group was seen. `90ff7faf` |
| 1686034 | High | Resource leak | `util/wmmenugen_parse_wmconfig.c`:142 (parse_wmconfig) | **fix** - Free wmc->Name/Exec/Category/Restart at the end of parse_wmconfig; wmc_to_wm() copies what it needs. `07e8278a` |

### Batch 4 - checked returns and bounds (14)

| cid | sev | type | location | action | commit |
|---|---|---|---|---|---|
| 1686112 | High | String not null terminated | `src/event.c`:Various (handle_inotify_events) | **harden** - Same bound check as 1686058; the name is kernel-padded with NULs, so this is provability, not a live overrun. `77476da0` |
| 1686105 | Medium | Dereference null return value | `src/dock.c`:Various (drawerRestoreState) | **harden** - Guard `dock_state` before WMGetFromPLDictionary(): a drawer entry can be missing from an old state file. `5c8f0e89` |
| 1686099 | Medium | Unchecked return value from library | `src/telemetry.c`:297 (wTelemetryInit) | **harden** - Check mkdir()'s result (EEXIST is the normal case) instead of ignoring it. `a39a845c` |
| 1686093 | Medium | Unchecked return value from library | `src/telemetry.c`:276 (rotate_spike_file) | **harden** - Check rename()'s result and log a debug message on failure. `7f048bb3` |
| 1686090 | Medium | Unchecked return value | `WPrefs.app/TexturePanel.c`:916 (SetTexturePanelTexture) | **harden** - Check XParseColor()'s result before using xcolor. `495d2372` |
| 1686088 | Medium | Unchecked return value | `src/wsmap.c`:487 (handle_event) | **harden** - Check XGrabPointer()'s status and abort the workspace map if the grab fails. `dc74ad08` |
| 1686083 | Medium | Dereference null return value | `src/dock.c`:1675 (wDockSaveState) | **harden** - Only call WMPutInPLDictionary() when WMGetFromPLDictionary(old_state, tmp) returned a dictionary. `8d2aefb7` |
| 1686076 | Medium | Unchecked return value from library | `src/startup.c`:Various (StartUp) | **harden** - Check the fcntl() results and warn/fall back if the self-pipe cannot be configured. `32f74ebb` |
| 1686062 | Medium | Unchecked return value from library | `src/misc.c`:474 (getTextSelection) | **harden** - Handle poll() == -1 (EINTR, EAGAIN) instead of ignoring the result. `1efd98d3` |
| 1686061 | Medium | Unchecked return value | `src/startup.c`:832 (manageAllWindows) | **harden** - Check XQueryTree()'s status before using children/nchildren. `5653af46` |
| 1686058 | High | Out-of-bounds read | `src/event.c`:401 (handle_inotify_events) | **harden** - Bound the parse loop (`i + sizeof(struct inotify_event) + pevent->len <= eventQLength`). Kernel events are well formed, this makes it provable. `77476da0` |
| 1686046 | High | Negative array index read | `WPrefs.app/WindowHandling.c`:Various (storeData) | **harden** - Clamp the popup index to >= 0 before indexing the option tables: WMGetPopUpButtonSelectedItem() returns -1 when nothing is selected. `f966b6db` |
| 1686045 | Medium | Dereference after null check | `util/xdnd.c`:933 (xdnd_drag) | **harden** - Return early (or take the documented default) when typelist is NULL instead of calling array_length(NULL). `b63a6453` |
| 1686035 | Medium | Unchecked return value from library | `WINGs/wcolorpanel.c`:3122 (customPaletteMenuRename) | **harden** - Check remove()'s result and warn: the palette list is updated even when the file was not deleted. `6fe3e9f4` |

### Batch 5 - hygiene and intent (15)

| cid | sev | type | location | action | commit |
|---|---|---|---|---|---|
| 1686111 | Medium | Logically dead code | `WINGs/dragsource.c`:375 (sendEnterMessage) | **verify** - `version` is assigned and unused in the >2 branch: check the intended XDND version negotiation before removing it. `456e73cc` |
| 1686107 | Medium | Dereference before null check | `src/session.c`:276 (makeWindowState) | **hygiene** - `command` is dereferenced before the `if (command)` test: drop the test or move it to where the string is built. `1b3f2abc` |
| 1686103 | High | Uninitialized pointer read | `util/wmiv.c`:1549 (connect_dir) | **hygiene** - Initialise `dir = NULL` before scandir(). `658207d6` |
| 1686095 | Low | Unused value | `src/rootmenu.c`:675 (constructMenu) | **hygiene** - Remove the `lpath = NULL` assignment. `95270e88` |
| 1686092 | Medium | Logically dead code | `src/event.c`:1975 (dispatchWKBDCommand) | **hygiene** - The branch is dead because shortcutWindows[widx] was already cleared on this path; remove it or explain why it stays. `89280a17` |
| 1686086 | remove the dead branch | the `MAX_OPTIONS` guard **is reachable** with 129 `--ignore` options; deleting it would let `ignoreList` overflow, so it is suppressed instead. |
| 1686065 | Medium | Overflowed constant | `WINGs/wcolorpanel.c`:3554 (getShift) | **hygiene** - Use `unsigned int i` as the counter and cast on return; the unsigned char wrap happens to give the right answer. `8de5810a` |
| 1686064 | Medium | Dereference before null check | `WINGs/wfilepanel.c`:652 (createDir) | **hygiene** - `file` comes from wmalloc and is never NULL: drop the redundant test. `35f7b64a` |
| 1686063 | Low | Unused value | `src/defaults.c`:Various (wDefaultsCheckDomains) | **hygiene** - Remove the dead `dict = NULL` assignment. `118aa256` |
| 1686060 | Medium | Dereference before null check | `src/wsmap.c`:116 (workspace_map_destroy) | **verify** - Check WMUnmapWidget(NULL): if it tolerates NULL, drop the later `if (wsmap->win)`; otherwise move the test before the call. `e202e58f` |
| 1686047 | Medium | Structurally dead code | `src/stacking.c`:493 (MoveInStackListAbove) | **hygiene** - Replace the for-with-break by an if: the increment is unreachable and hides that only the first candidate is used. `07d2e647` |
| 1255992 | Medium | Operands don't affect result | `src/xmodifier.c`:Various (x_reset_modifier_mapping) | **hygiene** - Same as 1255978, for meta_bit. `89b20252` |
| 1255987 | Medium | Operands don't affect result | `src/xmodifier.c`:Various (x_reset_modifier_mapping) | **hygiene** - Same as 1255978, for hyper_bit. `89b20252` |
| 1255982 | Medium | Operands don't affect result | `src/xmodifier.c`:Various (x_reset_modifier_mapping) | **hygiene** - Same as 1255978, for super_bit. `89b20252` |
| 1255978 | Medium | Operands don't affect result | `src/xmodifier.c`:Various (x_reset_modifier_mapping) | **hygiene** - `*(old_mod) != alt_bit` is trivially false when old_mod is &alt_bit: express the intent as `old_mod != &alt_bit` (same behaviour, no dead expression). `89b20252` |

### No change - suppressed (20)

| cid | sev | type | location | action | commit |
|---|---|---|---|---|---|
| 1686110 | Medium | Untrusted loop bound | `wrlib/load_ppm.c`:126 (pm_getuint) | **false positive** - The loop is bounded by EOF from the PPM stream; taint tracking has no security consequence in an image parser used by the same user. `d6944d8e` |
| 1686108 | Low | Calling risky function | `src/actions.c`:1355 (animateResize) | **accepted** - rand() picks a resize animation style. `975ded86` |
| 1686106 | Medium | Explicit null dereferenced | `WINGs/wscrollview.c`:454 (WMSetScrollViewHasVerticalScroller) | **false positive** - Same as 1686067, vertical scroller. `b6b51b78` |
| 1686102 | Low | Time of check time of use | `WINGs/proplist.c`:1865 (wrmdirhier) | **accepted** - nftw() tree removal of the user's own defaults directory. `e290b510` |
| 1686097 | Low | Time of check time of use | `WINGs/proplist.c`:1788 (wmkdirhier) | **accepted** - mkdir/stat race on the defaults hierarchy, same user. `e290b510` |
| 1686089 | Low | Time of check time of use | `util/setstyle.c`:440 (main) | **accepted** - setstyle reading a theme path given by the user. `e290b510` |
| 1686085 | Low | Calling risky function | `src/placement.c`:Various (randomPlaceWindow) | **accepted** - Random window placement; not security relevant. `975ded86` |
| 1686081 | Medium | Copy-paste error | `src/dock.c`:Various (killCallback) | **false positive** - WCHECK_STATE(WSTATE_NORMAL) followed by WCHANGE_STATE(WSTATE_MODAL) is the intended idiom in both callbacks. `d6944d8e` |
| 1686077 | Medium | Ignoring number of bytes read | `wrlib/load_ppm.c`:183 (load_graymap) | **false positive** - Tested with `if (!fread(buf, w, 1, file))`, same as 1686030. `d6944d8e` |
| 1686074 | Low | Time of check time of use | `WINGs/wcolorpanel.c`:3114 (customPaletteMenuRename) | **accepted** - Palette rename: the second access() path re-checks with WMRunAlertPanel. `e290b510` |
| 1686073 | Low | Time of check time of use | `WPrefs.app/Appearance.c`:Various (createPanel) | **accepted** - access() before mkdir() of WPrefs' own configuration directory. `e290b510` |
| 1686069 | Low | Time of check time of use | `WINGs/wfilepanel.c`:705 (deleteFile) | **accepted** - Delete-file dialog: stat() then unlink() in the user's own tree. `e290b510` |
| 1686067 | Medium | Explicit null dereferenced | `WINGs/wscrollview.c`:421 (WMSetScrollViewHasHorizontalScroller) | **false positive** - The caller clears flags.hasHScroller before reorganizeInterior(), which then skips the scroller block entirely. `b6b51b78` |
| 1686055 | High | Read from pointer after free | `WINGs/bagtree.c`:464 (WMEraseFromBag) | **false positive** - rbTreeDelete() assigns tree->root = x when the root is removed, so self->root is never the freed node. Separate real defect found here: WMEraseFromBag() does not renumber the following nodes' index (see batch 1). `d173750b` |
| 1686049 | Medium | Logically dead code | `util/wmsetbg.c`:565 (parseTexture) | **false positive** - The `break` is defensive; image is known non-NULL on the path that reaches it. `d6944d8e` |
| 1686044 | Low | Time of check time of use | `util/wmmenugen.c`:160 (main) | **accepted** - stat() on a path given on the command line; the tool runs with the caller's rights. `e290b510` |
| 1686042 | Low | Time of check time of use | `src/monitor.c`:160 (MonitorLoop) | **accepted** - The exit sentinel is created and consumed by the same user's WM in /tmp. `e290b510` |
| 1686041 | Low | Time of check time of use | `WINGs/wcolorpanel.c`:3028 (customPaletteMenuNewFromFile) | **accepted** - TOCTOU inside the user's own colour-palette directory: same trust domain, no privilege boundary. `e290b510` |
| 1686036 | Low | Calling risky function | `src/superfluous.c`:Various (DoKaboom) | **accepted** - rand() drives an icon animation; no security property depends on it. `975ded86` |
| 1686030 | Medium | Ignoring number of bytes read | `wrlib/load_webp.c`:82 (RLoadWEBP) | **false positive** - The result is tested (`if (!fread(buffer, sizeof(buffer), 1, file))`); the checker looks at the byte count, the code checks items. `d6944d8e` |

