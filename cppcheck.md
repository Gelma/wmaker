# Static Analysis Report (cppcheck)

Date: 2026-11-04  
Tool: `cppcheck 2.23 dev`  
Command line:
```bash
cppcheck -DHAVE_CONFIG_H -I. -I./src -I./wrlib -I./WINGs --enable=warning,performance,portability --inline-suppr -q src/ wrlib/ WINGs/ test/
```

---

## Executive Summary

- **`wrlib/`**: **0 warnings, 0 errors**. Completely clean following recent visual conversions and checked arithmetic additions.
- **`src/`**: 1 real Null Pointer Dereference warning identified in `src/defaults.c` / `src/screen.c`, plus minor portability int-to-pointer cast notices in callback tables.
- **`WINGs/`**: 1 real Null Pointer Dereference bug identified in `WINGs/widgets.c` (`WMDestroyScreen`), plus several false positives related to opaque macro iterations (`WM_ITERATE_ARRAY`).
- **`test/`**: 1 potential allocation failure unchecked dereference in `test/wm_stress.c`.

---

## Confirmed Real Bugs and Proposed Patches

### 1. `src/defaults.c`: Null Pointer Dereference via `wScreenUpdateRootEventMask(scr)`

#### Problem Description
In `src/defaults.c:1359-1368` inside `wReadDefaults(WScreen *scr, WMPropList *dict)`, the code explicitly checks and handles `scr == NULL` (which occurs when defaults are loaded or reloaded globally for all screens):

```c
if (scr && !scr->flags.key_grabs_suspended) {
    wScreenSetKeyGrabs(scr);
} else if (!scr) {
    int s_idx;
    for (s_idx = 0; s_idx < w_global.screen_count; s_idx++) {
        WScreen *s = wScreenWithNumber(s_idx);
        if (s && !s->flags.key_grabs_suspended)
            wScreenSetKeyGrabs(s);
    }
}
```

However, at line 1371, it immediately calls:
```c
wScreenUpdateRootEventMask(scr);
```
In `src/screen.c:87`, `wScreenUpdateRootEventMask(WScreen *scr)` unconditionally dereferences `scr->root_win`:
```c
void wScreenUpdateRootEventMask(WScreen *scr)
{
    long event_mask = EVENT_MASK;

    if (!(wPreferences.scrollable_menus || wPreferences.hot_corners))
        event_mask &= ~PointerMotionMask;

    XSelectInput(dpy, scr->root_win, event_mask);
}
```
If `scr == NULL`, calling `wScreenUpdateRootEventMask(scr)` causes an immediate segmentation fault (`SIGSEGV`).

#### Proposed Patch
```diff
--- a/src/defaults.c
+++ b/src/defaults.c
@@ -1368,7 +1368,14 @@ void wReadDefaults(WScreen *scr, WMPropList *dict)
 		}
 	}
 
-	wScreenUpdateRootEventMask(scr);
+	if (scr) {
+		wScreenUpdateRootEventMask(scr);
+	} else {
+		for (int s_idx = 0; s_idx < w_global.screen_count; s_idx++) {
+			WScreen *s = wScreenWithNumber(s_idx);
+			if (s)
+				wScreenUpdateRootEventMask(s);
+		}
+	}
 }
 
 void wReadKeybindings(WScreen *scr, WMPropList *dict)
```

---

### 2. `WINGs/widgets.c`: Crash on `NULL` in `WMDestroyScreen(WMScreen *scr)`

#### Problem Description
In `WINGs/widgets.c:1154-1174`, `WMDestroyScreen` initializes a stack array `pixmaps[]` by accessing member fields of `scr`:

```c
void WMDestroyScreen(WMScreen *scr)
{
	int i;
	W_Pixmap *pixmaps[] = {
		scr->checkButtonImageOn, scr->checkButtonImageOff,
		scr->radioButtonImageOn, scr->radioButtonImageOff,
		...
	};

	if (!scr)
		return;
```

Because the array `pixmaps[]` is initialized at function entry before the `if (!scr) return;` guard check, passing `scr == NULL` will dereference `scr` during stack initialization and crash immediately with `SIGSEGV`. The guard `if (!scr) return;` is currently dead code.

#### Proposed Patch
```diff
--- a/WINGs/widgets.c
+++ b/WINGs/widgets.c
@@ -1154,6 +1154,9 @@ void W_RedisplayWidget(WMWidget *w)
 void WMDestroyScreen(WMScreen *scr)
 {
 	int i;
+	if (!scr)
+		return;
+
 	W_Pixmap *pixmaps[] = {
 		scr->checkButtonImageOn, scr->checkButtonImageOff,
 		scr->radioButtonImageOn, scr->radioButtonImageOff,
@@ -1169,9 +1172,6 @@ void WMDestroyScreen(WMScreen *scr)
 		scr->applicationIconPixmap
 	};
 
-	if (!scr)
-		return;
-
 	if (scr->sharedOpenPanel) {
 		WMFreeFilePanel(scr->sharedOpenPanel);
 		scr->sharedOpenPanel = NULL;
```

---

### 3. `test/wm_stress.c`: Missing NULL check on `calloc()`

#### Problem Description
In `test/wm_stress.c:363-366`, `malloc` and `calloc` are called without checking for allocation failures before indexing:

```c
pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
WorkerArg *args = calloc(num_threads, sizeof(WorkerArg));

for (int i = 0; i < num_threads; i++) {
    args[i].thread_id = i;
...
```

#### Proposed Patch
```diff
--- a/test/wm_stress.c
+++ b/test/wm_stress.c
@@ -362,6 +362,11 @@ int main(int argc, char **argv)
 
 	pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
 	WorkerArg *args = calloc(num_threads, sizeof(WorkerArg));
+	if (!threads || !args) {
+		fprintf(stderr, "Fatal: failed to allocate memory for worker threads\n");
+		free(threads);
+		free(args);
+		return 1;
+	}
 
 	for (int i = 0; i < num_threads; i++) {
 		args[i].thread_id = i;
```

---

## False Positives Analyzed

1. **`WINGs/selection.c` and `WINGs/wfontpanel.c` (`uninitvar`)**:
   `cppcheck` reports variables inside `WM_ITERATE_ARRAY(array, var, i)` as uninitialized. This is an artifact of cppcheck not inspecting the internal semantics of `WMArrayFirst()`. In reality, `WM_ITERATE_ARRAY` expands to:
   ```c
   for (var = WMArrayFirst(array, &(i)); (i) != WANotFound; var = WMArrayNext(array, &(i)))
   ```
   where `var` is explicitly assigned before loop condition evaluation. No action needed.

2. **`src/defaults.c` (`portability: intToPointerCast`)**:
   Casting small integer constants (`(void *)WM_CLIP`, `(void *)M_QUICK`) as callback client data is an established X11/WINGs idiom. While `(void *)(uintptr_t)` is strictly more portable, these casts are safe on all LP64 and ILP32 platforms.
