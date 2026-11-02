# SpecAI - Window Maker Project Specifications and Status

## Project Overview
- **Project**: Window Maker 0.96.0 (Window Manager for the X Window System, NeXTSTEP / GNUstep style).
- **OS Environment**: Ubuntu 24.04 LTS (x86_64, Linux).
- **Custom Installation Directory**: `/opt/wmaker`.
- **Language Convention**: Always write all documentation (`SpecAI.md`) and user responses in English.

---

## Objectives and Completed Activities

### 1. Build and Installation Script (`build_and_install.sh`)
- Created the executable script [build_and_install.sh](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/build_and_install.sh).
- **Script Features**:
  - Automatic check and installation of required packages/dependencies on Ubuntu 24.04 (`libx11-dev`, `libxext-dev`, `libxinerama-dev`, `libxrandr-dev`, `libxpm-dev`, `libxft-dev`, `libxmu-dev`, `libpango1.0-dev`, `libpng-dev`, `libjpeg-dev`, `libgif-dev`, `libtiff-dev`, `libwebp-dev`, `libexif-dev`, `libarchive-dev`, `autoconf`, `automake`, `libtool`).
  - Target directory management for `/opt/wmaker` (creation and ownership assigned to the current user).
  - Configuration with `/opt/wmaker` prefix and X extensions support (Xinerama, RandR, Shape, Shm, Pango, full image format support).
  - Optimized parallel compilation with `-march=native -O2 -g -Wall` flags.
  - Full installation into `/opt/wmaker`.
  - Creation of the `/opt/wmaker/bin/wmaker-session` session wrapper with automatic configuration of `PATH`, `LD_LIBRARY_PATH`, and `XDG_DATA_DIRS`.
  - Registration of the X11 session for the Display Manager in `/usr/share/xsessions/wmaker-opt.desktop`.

### 2. Lifecycle Control Scripts
- [start.sh](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/start.sh): Starts Window Maker from `/opt/wmaker` in the current X11 session or initializes an `xinit` display if none is running.
- [stop.sh](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/stop.sh): Cleanly stops running Window Maker processes by sending the appropriate signal (`SIGTERM`).

### 3. Patches and Bug Fixes Applied to the Codebase
All fixes have been applied in individual, incremental Git commits, each signed with `Made-by-IA: Gemini`:

1. **[`74351e8b`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/wmspec.c#L990-L1015) - `wmspec`: Fix `sizeof(pointer)` in `updateWorkspaceNames`**:
   - `sizeof(pos)` returned 8 bytes (pointer size on 64-bit architectures), truncating the copy to 7 bytes and leaving `_NET_DESKTOP_NAMES` corrupted.
   - Replaced with a safe copy bounded by `MAX_WORKSPACENAME_WIDTH` and explicit null-termination.
2. **[`61e5fc9e`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/dock.c#L4745-L4870) - `dock`: Guard against stack corruption and VLAs in Drawers and Menus**:
   - Replaced stack VLA buffers (`WAppIcon *aicons_to_shift[drawer->icon_count]` and `Window wins[n]`) with safe dynamic heap allocations sized to `max_icons`.
   - Added defensive bounds checking on `wins[idx]` to prevent out-of-bounds stack writes.
   - In `updateWorkspaceMenu()`: moved the `!menu` null-check before accessing `menu->frame`.
   - In `makeWorkspaceMenu()`: added an immediate `return NULL` if `wMenuCreate()` fails.
3. **[`2fb52eaf`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/actions.c#L198-L215) - `actions`: NULL check on `focused` in `wSetFocusTo`**:
   - When `scr->focused_window` is NULL (e.g., when closing the last window in a workspace), `focused->next` caused a `SIGSEGV`. Added the defensive guard `if (focused) focused->next = wwin;`.
4. **[`d61746b3`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/client.c#L105-L150) - `client`: Default case in `wClientGetGravityOffsets`**:
   - Added a `default` branch to initialize `*ofs_x = 0; *ofs_y = 0;` when non-standard `win_gravity` values are sent by clients.
5. **[`a474b925`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/wtext.c#L1175-L1190) - `WINGs`: Early return on NULL text block in `wtext.c`**:
   - Removed the `...for this app will surely crash` comment/printf and added `return;` to avoid dereferencing `tb->sections[s]` when `tb` is NULL.
6. **[`8e56bf09`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/wballoon.c#L375-L395) - `WINGs`: NULL pointer arithmetic in `wballoon.c`**:
   - When `strchr` did not find a `\n`, `ptr2` became NULL and `ptr = ptr2 + 1` performed NULL pointer arithmetic.
7. **[`223810ed`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/WINGs/WINGs.h#L965-L975) - `WINGs`: Align parameter order in headers**:
   - Synchronized the `WMSetViewExpandsToParent` prototype with its actual definition (`leftOffs` before `topOffs`), as well as the parameters of `W_SendDnDClientMessage` in `WINGsP.h.in`.
8. **[`35006a0f`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/session.c#L438-L445) - `session`: `%u` format specifier in `sscanf`**:
   - Fixed the format specifier from `%i` to `%u` for the `unsigned int w, h` fields of `WSavedState`.
9. **[`b58f0abe`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/rootmenu.c#L510-L525) - `rootmenu`: NULL check on `line` in `next_token`**:
   - Prevented immediate dereference of `*line` when passed a NULL pointer.
10. **[`a9f0042e`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/menu.c#L285-L300) - `menu`: Defensive checks in `wMenuEntrySetCascade`**:
    - Added an early guard `if (!menu || !entry || !cascade) return;`.
11. **[`c431060f`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/WINGs/WUtil.h#L525-L530) - `WINGs`: Align parameter names in `WMAppendBag`**:
    - Synchronized parameter names between prototype (`WUtil.h`) and definition (`bagtree.c`).
12. **[`306917c9`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/dock.c#L2005-L4525) - `dock`: NULL checks on `old_top` and single appicon removal in drawer**:
    - In `wDockRestoreState()`: checked `old_top != NULL` before updating default icon coordinates.
    - In `removeDrawerCallback()`: checked `aicon != NULL` before moving the remaining appicon back to the main dock.
13. **[`8e562f7d`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/workspace.c#L900-L915) - `workspace`: Truncate workspace names restored from WMState**:
    - Used `wstrndup()` bounded by `MAX_WORKSPACENAME_WIDTH` to prevent overly long workspace names in configuration files from overflowing Window Maker buffers.
14. **[`588e572d`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/util/wxpaste.c#L80-L150) - `wxpaste`: Fix event loop and uninitialized variable**:
    - In `fetchSelection()`, the wait loop for the `SelectionNotify` event was erroneously written as `while (0)`, skipping reception completely and leaving `res` uninitialized.
    - In `main()`, the length variable `l` passed to `write(STDOUT_FILENO, buf, l)` was not set by `fetchSelection()`, causing random bytes to be written to stdout.
    - Fixed the loop to `while (1)`, initialized `res = 0`, returned the actual property length in `l`, and initialized `l = 0`.
15. **[`4d8a5118`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/wtext.c#L1170-L1185) - `WINGs`: Preventive NULL check on `tb` in `wtext.c`**:
    - When jumping to `_doneH:` inside `findClosestPositionTo()`, `tb->graphic` was dereferenced before the `if (!tb) return;` check located under `_doNothing:`.
    - Moved the validity check `if (!tb) return;` to the start of the `_doneH:` block, preventing NULL pointer dereferences in text widgets.
16. **[`45de612a`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/Extras/wtableview.c#L870-L895) - `WINGs`: Initialize `range` struct in `columnsInRect`**:
    - When `totalColumns == 0` or no column intersected the rectangle, `range.position` and `range.count` remained uninitialized, causing out-of-bounds accesses in `drawRow()`.
    - Initialized `range.position = 0; range.count = 0;` with an early return if no columns exist or match.
17. **[`d0053942`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/appmenu.c#L130-L140) - `appmenu`: Width limit in `sscanf` to prevent stack overflow**:
    - When parsing the X11 application menu in `parseMenu()`, an unbounded `%s` read a string from a client window into the stack buffer `char rtext[300]`.
    - Added the `%299s` width limit to prevent stack buffer overflows from malicious or corrupted payloads sent by X clients.
18. **[`c4172978`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/xdnd.c#L80-L86), [`b22c8da1`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/util/wmiv.c#L1595-L1605), [`26370998`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/util/setstyle.c#L225-L235), [`cfe35312`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WPrefs.app/Appearance.c#L1315-L1330) - Fix `%x`/`%X` types in `sscanf`/`fscanf`**:
    - Resolved type mismatches where pointers to signed integers (`int *`) were passed to hexadecimal specifiers `%x`/`%2X`, which strictly require `unsigned int *`.
19. **[`ae59afa4`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/wcolorpanel.c#L1890-L3490) - `WINGs`: Initialize `CPColor` struct, fix format and base length in Color Panel**:
    - Initialized all local stack instances of `CPColor` with `{ 0 }` in callbacks to avoid garbage values in `cpColor.hsv` when only `cpColor.rgb` is set.
    - Fixed a potential buffer overflow in `hsbInit()` by enlarging `tmp` to 16 bytes and using `snprintf()` with `%d` instead of `%hu`.
    - In `generateNewFilename()`, checked for a space before `{` to prevent negative indices and buffer underflow in `strncpy()` on specific color names.
20. **[`bb0727d2`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/dialog.c#L1157-L1160) - Align `printf`/`snprintf` format specifiers and buffer sizes**:
    - `src/dialog.c`: Fixed `%hu` to `%u` for the current year in `COPYRIGHT_TEXT`.
    - `src/dock.c`: Used `%d` for `current_workspace` and enlarged the `ws_number` buffer to 32 bytes to eliminate truncation warnings.
    - `src/geomview.c`: Replaced `%+5i` with `%5u` for unsigned width and height dimensions.
    - `src/session.c`: Replaced `%ix%i` with `%ux%u` to store unsigned width and height.
    - `WINGs/wruler.c`: Enlarged the `c` buffer to 16 bytes and fixed `%hu` to `%d` with dynamic `strlen(c)`.
21. **[`b4499511`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/client.c#L310-L320) - `client`: NULL guards in `wClientCheckProperty`**:
    - Added an early check `if (!wwin || !event) return;` to avoid immediate dereferencing of `wwin->flags` when handling a `PropertyNotify` event on unassociated or invalid windows.
22. **[`c7dfaef1`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/event.c#L732-L1590) - `event`: Verify `main_window_desc` pointer in event handlers**:
    - In `handleDestroyNotify`, `handlePropertyNotify`, and the `WKBD_HIDE` keybinding handler, verified that `wapp->main_window_desc` is non-NULL before accessing `screen_ptr` or `WFLAGP`, preventing Window Maker crashes when an X11 application terminates abnormally.
23. **[`1b03ad80`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/actions.c#L2014-L2018) - `actions`: NULL check in `wUnhideApplication`**:
    - Added the guard `if (!wapp || !wapp->main_window_desc) return;` to avoid NULL dereference of `wapp->main_window_desc->screen_ptr`.
24. **[`d8f5f4cd`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/winmenu.c#L638) - `winmenu`: NULL guard on `main_window_desc` in window menu**:
    - In `updateMenu()`, checked `wapp->main_window_desc` before evaluating the `WFLAGP(wapp->main_window_desc, no_appicon)` macro, avoiding crashes when opening the window commands menu.
25. **[`6d46cd4b`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/window.c#L1329) - `window`: NULL check on `main_window_desc` in `wManageWindow`**:
    - Checked `app->main_window_desc != NULL` before accessing the `start_hidden` attribute.
26. **[`8dcb6567`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/wraster.h.in#L65) - `wrlib`: Variadic `__wrlib_nonnull` macro in `wraster.h.in` template**:
    - In the fallback template, `#define __wrlib_nonnull(argidx)` accepted only a single argument instead of `(...)`, causing macro expansion and static analysis parsers to fail. Fixed to variadic `__wrlib_nonnull(...)`.
27. **[`99a11f75`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/util/xdnd.c#L525-L545) - `xdnd`: Fix out-of-bounds pointer arithmetic and add heap allocation checks**:
    - In `util/xdnd.c:526`, `*descriptions` has type `char **`. The statement `memcpy(*descriptions + l, data, dcount)` added the offset multiplied by `sizeof(char *)`, causing severe heap corruption and stack smashing. Fixed by casting to byte pointer `(char *)*descriptions + l`.
    - Added allocation failure checks on `*descriptions` and `*types`.
28. **[`2873c117`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/util/wmmenugen_parse_wmconfig.c#L105) - `wmmenugen`: NULL guard on `label` in `parse_wmconfig_file`**:
    - If a configuration line did not provide a label (`label == NULL`), calling `strcmp(lastlabel, label)` caused `wmmenugen` to crash immediately. Added the `if (label)` guard condition.
29. **[`14f5d37e`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/session.c#L188-L195) - `session`: Fix memory leak and potential NULL crash on window class**:
    - Reassigning `class = strdup(wwin->wm_class)` leaked the pointer allocated by `XGetClassHint` and did not check `wwin->wm_class != NULL`. Replaced with a safe temporary `const char *c` pointer.
30. **[`9dc319ea`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/superfluous.c#L145-L155) - `superfluous`: Check `malloc` return value in `wAppBounce`**:
    - Verified memory allocation for the `points` buffer before entering the animation calculation loop.
31. **[`83ebd261`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/window.c#L1840) - `window`: Clean up redundant `focus_mode` check in `wUnmanageWindow`**:
    - Removed duplicate nested check `focus_mode == WKF_CLICK` already guaranteed by the outer condition.
32. **[`5625724f`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/main.c#L365) - `main`: Use `wmalloc` for `wVisualID`**:
    - Replaced unchecked `malloc()` call with the safe `wmalloc()` wrapper.
33. **[`0a48ac89`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/xdnd.c#L445) - `xdnd`: Allocation failure check in `wXDNDGetTypeList`**:
    - Checked allocation of the `typelist` array to prevent NULL dereference under OOM conditions.
34. **[`22f090e0`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/dialog.c#L1158) - `dialog`: Align `%d` format specifier for `current_year`**:
    - Replaced unsigned specifier `%u` with `%d` for signed integer in `COPYRIGHT_TEXT`.
35. **[`58f2a261`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/menuparser_macros.c#L205) - `WINGs`: `%u` specifier for unsigned count in `menuparser_macros`**:
    - Fixed format specifier from `%d` to `%u` for an `unsigned int` variable.
36. **[`757e51a2`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/scale.c#L740-L750) - `wrlib`: Check memory allocations in `RSmoothScaleImage`**:
    - Verified proper allocation of raster lines `wlines[0]` and `wlines[1]` to prevent crashes when scaling images under low-memory conditions.
37. **[`f3fd4576`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/Tests/wtest.c#L360) - `WINGs/Tests`: Close file descriptor and convert encoding to UTF-8**:
    - Added `fclose(f)` inside the test file reading loop and converted a Latin-1 accented character to valid UTF-8.
38. **[`42bdb767`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/gradient.c#L47) - `wrlib`: NULL check on `colors` array in `RRenderMultiGradient`**:
    - Added `if (!colors) return NULL;` guard before looping through gradient stops.
39. **[`f2a3be62`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/string.c#L209) - `WINGs`: Simplify null pointer checks in `wstrconcat`**:
    - Streamlined the pointer guard sequence for concatenated strings.
40. **[`4b7b4e79`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/wtext.c#L355-L2085) - `WINGs`: Wrap debug `printf` calls in `DEBUG` macro and clean up logic**:
    - Wrapped low-level diagnostic logging statements in `#ifdef DEBUG` and fixed unsigned `%u` specifier for `tb->first`.
    - Removed redundant conditions on `tb->graphic` and simplified text alignment assignment.
41. **[`84cca8a3`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/memory.c#L195) - `WINGs`: Remove dead assignment to `ptr` in `wfree`**:
    - Removed `ptr = NULL;` at the end of `wfree(void *ptr)` since the parameter is passed by value, has no external effect, and was flagged by Cppcheck as `uselessAssignmentPtrArg`.
42. **[`053afe12`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/wtext.c#L1660) - `WINGs`: `%u` specifier for `nitems` in `wtext.c`**:
    - Fixed format specifier from `%d` to `%u` for the `unsigned int nitems` variable in the diagnostic `printf`.
43. **[`ec434c8b`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/tests/testgrad.c#L109-L120) - `wrlib/tests`: Check `malloc` return value in gradient tests**:
    - Verified allocation of the `colors` array and individual `colors[i]` structures in `testgrad.c` prior to dereferencing.
44. **[`199419bd`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/WINGs/WUtil.h#L48-L208) - `WINGs`: Protect assertion macros with `do-while` and parenthesize `WMScale`**:
    - Wrapped assertion macros `wassertr` and `wassertrv` in `do { ... } while (0)` blocks to prevent dangling-else bugs when used in `if-else` statements.
    - Added parentheses around `(value)` in `(double)(value)` inside `WMScaleX` and `WMScaleY` macros to prevent operator precedence bugs when complex arithmetic expressions are passed.
45. **[`d54c888c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/WindowMaker.h#L261-L274) - `wmaker`: Wrap `WCHANGE_STATE` and `SIG_WCHANGE_STATE` in `do-while`**:
    - Replaced bare `{ ... }` compound blocks with safe `do { ... } while (0)` constructs.
46. **[`bff27388`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/xdnd.h#L33-L42) - `xdnd`: Parenthesize index parameter in `XDND_ENTER_TYPE` and `do-while` for `XDND_STATUS_RECT_SET`**:
    - In `src/xdnd.h` and `util/xdnd.h`, fixed `2 + i` to `2 + (i)` and wrapped `XDND_STATUS_RECT_SET` in `do { ... } while (0)`.
47. **[`819e62e8`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/window.h#L83) - `window`: Parenthesize comma expression in `WSETUFLAG`**:
    - Enclosed `((wwin)->user_flags.FLAG = (VAL), ...)` in parentheses to prevent operator precedence issues with ternary or assignment operators.
48. **[`84e8f018`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/stacking.h#L32) - `stacking`: Remove obsolete `CommitStackingForFrame` prototype**:
    - Removed unimplemented declaration historically superseded by `CommitStackingForWindow`.
49. **[`fdf528c5`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/workspace.h#L40) - `workspace`: Remove orphaned `wWorkspaceMenuEdit` prototype**:
    - Removed prototype not defined in any translation unit.
50. **[`64d03409`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/texture.h#L168-L177) - `texture`: Remove unimplemented prototypes and harden `FREE_PIXMAP`**:
    - Removed unimplemented prototypes `wTexturePaint`, `wTextureRender`, and `wTexturePaintTitlebar`.
    - Wrapped the `FREE_PIXMAP` macro in a safe `do { ... } while (0)` block.
51. **[`e907a7c3`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/defaults.h#L39) - `defaults`: Remove dead `wSaveDefaults` prototype**:
    - Removed unimplemented declaration.
52. **[`6a09948f`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/xutil.h#L25-L28) - `xutil`: Remove orphaned `RequestSelection` and `GetSelection` prototypes**:
    - Removed leftover declarations from legacy X11 selection code.
53. **[`ec224d35`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/keybind.h#L195) - `keybind`: Remove dead `wKeyboardInitialize` prototype**:
    - Removed prototype undefined in `src/misc.c` or anywhere else in the project.
54. **[`e09bbed4`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/WINGs/WINGsP.h.in#L332) - `WINGs`: Remove obsolete `W_FreeDragOperationItem` prototype**:
    - Removed unimplemented prototype in the drag-and-drop module.
55. **[`510eb8c4`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/menu.c#L553-L573) - `menu`: Cancel `jump_back` timer on menu destruction (Fix Segfault in `wMenuMove` at offset `0x4a903`)**:
    - Analyzed the kernel crash (`segfault at 51 ip 000055555559e903 ... in wmaker[4a903]`), pinpointed to `wMenuMove` (`src/menu.c:2126`) while dereferencing `menu->frame->core->window` on an already-freed `WMenu` structure (use-after-free).
    - `wMenuScroll()` registered a 400ms timer (`MENU_JUMP_BACK_DELAY`, `callback_leaving`) passing `menu->jump_back`, whereas `wMenuDestroy()` only cancelled `menu->timer` and forgot `menu->jump_back`. If the menu was destroyed before the 400ms elapsed, `callback_leaving` invoked `wMenuMove(dl->menu, ...)` on the freed menu.
    - Fixed `wMenuDestroy` to cancel and free `menu->jump_back`, and fixed `wMenuScroll` when `jump_back` is cleared by clicking the titlebar.
56. **[`e999a4bd`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/winmenu.c#L849-L879) - `winmenu`: Reset `scr->workspace_submenu` and clean up `rtext` in `DestroyWindowMenu`**:
    - `DestroyWindowMenu()` recursively destroyed `scr->window_menu` (including `scr->workspace_submenu`) without resetting `scr->workspace_submenu = NULL` and without freeing the shortcut `rtext` strings in the options submenu beforehand (preventing a double-free with `setModifierKeyLabels`/`setModifierShortKeyLabels`).
57. **[`108f91eb`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/rootmenu.c#L1623-L1687) - `rootmenu`: Prevent destroying `scr->root_menu` while active/mapped and rebuild `wKeyTreeRoot`**:
    - Prevented `wRootMenuReparse()` from destroying `scr->root_menu` (e.g., upon an `inotify` event in `wDefaultsCheckDomains`) while the menu or any of its submenus/brothers is mapped or inside `menuMouseDown` (`flags.inside_handler`), avoiding a use-after-free in `menuMouseDown` -> `wMenuMove(parentMenu(desc->parent), ...)`.
    - Added a call to `wKeyTreeRebuild(scr)` whenever root menu shortcuts change in `wRootMenuReparse()`, `OpenRootMenu()`, and `executeButtonAction()`, avoiding dangling `WMenu *` / `WMenuEntry *` pointers in the `wKeyTreeRoot` tree.
58. **[`55a742a3`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/menu.c#L604-L2189) - `menu`: Harden `wMenuMove`, `wMenuDestroy`, and `menuMouseDown` against NULL or dangling pointers**:
    - Added guards on `menu->frame`, `menu->frame->core`, `selected_entry` bounds, and `menu->cascades[i] != NULL` in `wMenuMove`, `selectEntry`, `closeBrotherCascadesOf`, `parentMenu`, `raiseMenus`, `keyboardMenu`, `saveMenuRecurs`, and `restoreMenuRecurs`.
    - In `wMenuDestroy(menu, False)`, unlinked `menu->cascades[i]->parent = NULL` to prevent dangling `parent` pointers in undestroyed submenus.
    - In `menuMouseDown()`, avoided overwriting `desc` when opening the window menu via right-click on `scr->switch_menu`, ensuring `flags.inside_handler` is properly cleared.
59. **[`806206a5`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/defaults.c#L1424-L1442) - `defaults`, `event`: Fix Use-After-Free in key-chain trie rebuild and action dispatch**:
    - In `wKeyTreeRebuild()`, cancelled any active key-chain timeout timer (`w_global.shortcut.chain_timeout_handler`), released the keyboard grab, and reset `w_global.shortcut.curpos` to `NULL` before destroying `wKeyTreeRoot`, preventing `curpos` from becoming a dangling pointer if the key trie is rebuilt mid-keychain.
    - In `handleKeyPress()` (`src/event.c`), snapshotted `match->actions` into a local array and retained `wwin` before invoking menu callbacks or `dispatchWKBDCommand()`, preventing Use-After-Free when an action (such as `WKBD_ROOTMENU`) rebuilds the key trie or closes the focused window mid-loop.
60. **[`f31c0744`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/rootmenu.c#L747-L875) - `rootmenu`, `event`: Fix Use-After-Free in dynamic root submenus and `switch_menu` replacement**:
    - Rebuilt the key-binding trie (`wKeyTreeRebuild`) and rebound key grabs (`rebindKeygrabs`) in `constructMenu()` and `constructPLMenuFromPipe()` whenever replacing an `OPEN_MENU` / `OPEN_PLMENU` cascade changes root-menu shortcuts, preventing `wKeyTreeRoot` from retaining dangling `WMenu *` and `WMenuEntry *` pointers.
    - Destroyed any existing standalone `scr->switch_menu` (`scr->switch_menu->parent == NULL`) in `addWindowsMenu()` before overwriting `scr->switch_menu`, preventing a mapped standalone window list menu from keeping dangling `WWindow *` pointers when windows later close.
    - Guarded `OpenRootMenu()` and `executeButtonAction()` with `wRootMenuIsInUse()`, and preserved `added_workspace_menu` / `added_windows_menu` flags when `scr->root_menu` is not replaced.
61. **[`3683df72`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/dock.c#L204-L1435) - `dock`, `appicon`, `dockedapp`, `superfluous`: Fix Use-After-Free in dock/clip/drawer timers and modal appicon dialogs**:
    - Cancelled `auto_collapse_magic`, `auto_expand_magic`, `auto_lower_magic`, and `auto_raise_magic` timers in `wDockDestroy()` and `drawerDestroy()` before freeing the dock/drawer structure.
    - Called `WMDeleteTimerWithClientData(aicon)` in `wAppIconDestroy()` to cancel pending `dockIconPaint` launch timers targeting a destroyed appicon.
    - In `appicon.c:killCallback()`, captured and retained `wwin = wapp->main_window_desc` before `wMessageDialog()` instead of dereferencing `wapp` after the modal loop returns (when `wapp` may have already been destroyed).
    - Retained `WAppIcon` across modal dialogs in `dock.c` (`killCallback`, `removeIconsCallback`, `keepIconsCallback`) and `dockedapp.c` (`ShowDockAppSettingsPanel`, `panelBtnCallback`, `DestroyDockAppSettingsPanel`), re-evaluating selected icons after confirmation dialogs return.
    - Cleaned up the persistent bounce timer, `bouncing` flag, and `WApplication` reference in `doAppBounce()` (`src/superfluous.c`) if `data->wapp->app_icon` becomes `NULL`.
62. **[`7520cc4b`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/window.c#L1653-L1774) - `stacking`, `window`, `wmspec`, `balloon`, `dialog`, `winspector`, `switchpanel`, `cycling`: Fix Use-After-Free in window lifecycle, stacking, NETWM, inspector, and cycling**:
    - Cleared dangling `wlist->stacking->child_of` pointers in `RemoveFromStackList()` (`src/stacking.c`) when an owner frame is removed from the stack list.
    - In `wUnmanageWindow()` (`src/window.c`), cancelled `scr->autoRaiseTimer` when `scr->autoRaiseWindow` matches the unmanaged frame, and cleared `oapp->last_focused` when the unmanaged window was the application's `last_focused` window.
    - In `wmspec.c`, removed unmanaged windows from `ndata->show_desktop` upon `WMNUnmanaged` notifications to prevent `wDeiconifyWindow()` from dereferencing freed `WWindow *` pointers when leaving `_NET_SHOWING_DESKTOP` mode; also cleaned up `scr->netdata` and its observers in `wNETWMCleanup()` and guarded `NULL` `src_image`.
    - In `destroyInfoPanel()` (`src/dialog.c`), passed `infoPanel->win` instead of `infoPanel` to `WMUnmapWidget()` to prevent type-confusion memory corruption of `infoPanel->wwin`.
    - In `winspector.c`, removed notification observers in `wDestroyInspectorPanels()` before unmanaging, and guarded `showIconFor()` and `applySettings()` modal `wMessageDialog()` calls with `choosingIcon`/`destroyed` checks.
    - In `switchpanel.c` and `cycling.c`, retained `WWindow` references while the switch panel is open and verified windows remain managed before focusing or raising them during `Alt+Tab` cycling.
    - In `balloon.c`, unconditionally cancelled `scr->balloon->timer` in `wBalloonHide()` even when the balloon is currently mapped, and guarded `NULL` `aicon->wm_instance` for GNUstep appicons.
63. **[`5f4bf48f`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/dialog.c#L1144-L1765) - `client`, `cycling`, `dialog`, `switchpanel`, `window`: Fix dangling pointer dereferences and resource leaks in window lifecycle and dialogs**:
    - In `switchpanel.c` and `cycling.c`, avoided dereferencing unverified `WWindow *` pointers before verifying they remain in the screen's live window list; also guarded `sameWindowClass()` against checking `wm_class` if either window has `flags.destroyed` set (since `wWindowDestroy` frees `wm_class` while the struct is kept alive by `wretain`).
    - In `window.c`, explicitly reset `normal_hints`, `wm_hints`, `wm_instance`, `wm_class`, `wm_gnustep_attr`, and `cmap_windows` to `NULL` upon freeing them in `wWindowDestroy()`, ensuring any retained references do not hold dangling member pointers.
    - In `dialog.c`, recorded the parent X11 window in `InfoPanel` and `LegalPanel` structs and destroyed them on panel closure via `XDestroyWindow(dpy, parent)` to eliminate orphaned X11 window leaks; also released the logo pixmap with `WMReleasePixmap(logo)` in `wShowCrashingDialogPanel()`.
    - In `client.c`, reset `wapp->menu = NULL` immediately after calling `wAppMenuDestroy(wapp->menu)` when handling `WM_COMMAND` property updates.
64. **[`6e2d2065`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wmaker-opt-valgrind.desktop#L1-L7) - `wmaker-opt-valgrind.desktop`, `wmaker-valgrind-session`: Valgrind supervision environment for /opt/wmaker**:
    - Created `wmaker-valgrind-session` wrapper script with strict Bash directives (`set -euo pipefail`), launching `/opt/wmaker/bin/wmaker` under Valgrind Memcheck with `--trace-children=yes` and `--trace-children-skip='/bin/*,/usr/bin/*,!/opt/wmaker/bin/wmaker'` to follow the inner child `wmaker` process while skipping dock/menu apps.
    - Automatically creates and redirects Valgrind memory diagnostic reports and stdout/stderr to `~/.local/share/wmaker/logs/valgrind-wmaker-<TIMESTAMP>-<PID>.log` and `wmaker-<TIMESTAMP>.log`.
    - Created and added `wmaker-opt-valgrind.desktop` matching `/usr/share/xsessions/wmaker-opt.desktop` with target `Exec=/opt/wmaker/bin/wmaker-valgrind-session`. Installed the runner script to `/opt/wmaker/bin/wmaker-valgrind-session`.
65. **[`4f9838d8`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/monitor.c#L76-L173) - `monitor`: Forward lifecycle signals to child process in `MonitorLoop`**:
    - In `MonitorLoop()` (`src/monitor.c`), registered signal handlers for `SIGHUP`, `SIGTERM`, `SIGINT`, `SIGQUIT`, `SIGUSR1`, and `SIGUSR2` in the parent monitor process to forward them via `kill()` to the active child `wmaker` process. Handled `EINTR` in `waitpid()`.
    - Prevented the monitor process from crashing or leaking child argument memory upon receiving session termination signals (`SIGHUP`/`SIGTERM` from GDM/X11), allowing graceful child shutdown and `child_argv` freeing.
66. **[`de93034c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/defaults.c#L1040-L1057) - `defaults`: Release unmerged global PropList in `wDefaultsInitDomain`**:
    - In `wDefaultsInitDomain()` (`src/defaults.c`), fixed a leak of the system global domain `shared_dict` (e.g. `PKGCONFDIR/WMRootMenu`, ~13 KB) when the user domain was already loaded as an array (not a dictionary). Added an explicit `WMReleasePropList(shared_dict)` in the unmerged branch, achieving 0 definitely lost blocks on clean exit.
67. **[`41742a92`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/actions.c#L2331-L2370) - `actions`, `framewin`, `screen`, `switchmenu`, `window`, `winmenu`, `workspace`: Fix active session memory leaks and Use-After-Free crashes**:
    - In `wWorkspaceMenuUpdate()` (`src/workspace.c`) and `updateWorkspaceMenu()` (`src/winmenu.c`), freed existing `entry->rtext` shortcut labels before reassigning new ones, eliminating thousands of repetitive leaked heap blocks during workspace switching.
    - In `wFrameWindowUpdateBorders()` and `wFrameWindowDestroy()` (`src/framewin.c`), freed previously allocated `fwin->border_pixel`, `fwin->focused_border_pixel`, and `fwin->selected_border_pixel` heap blocks, and freed RGB description strings returned by `WMGetColorRGBDescription()`. Also freed color strings in `wScreenInit()` (`src/screen.c`).
    - In `wWindowGetLanguageLabel()` (`src/window.c`), freed all dynamically allocated string members (`rules`, `model`, `layout`, `variant`, `options`) returned by `XkbRF_GetNamesProp()`.
    - Implemented `wWindowIsManaged()` in `src/window.c` and `src/window.h`. In `wWindowSingleFocus()` (`src/window.c`) and `wMakeWindowVisible()` (`src/actions.c`), retained window references and checked `wWindowIsManaged(scr, wwin)` and `!wwin->flags.destroyed` before and after workspace switching or unhiding applications, preventing Use-After-Free when client windows close during `ProcessPendingEvents()`.

---

## Verification Status
- Build scripts and `make` commands executed successfully (exit code 0).
- All compiled Window Maker binaries and libraries are installed and up to date in `/opt/wmaker`.
- `build_and_install.sh`, `start.sh`, `stop.sh`, and `wmaker-valgrind-session` are operational and validated.
