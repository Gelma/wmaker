# Breaking to the Future — Step-by-Step Modernization Plan

This document outlines the step-by-step technical implementation plan for modernizing Window Maker, leveraging modern toolchains (**GCC 14+ / Clang 23+**), the **C23 / GNU23** language standard, modern Linux kernel primitives (kernel 6.x+), and modern X11 extensions (**XRender / XCB**).

Backward compatibility with pre-C99 compilers and legacy 1990s visual architectures is intentionally broken in exchange for maximum performance, minimal CPU cycles, reduced syscall overhead, and robust memory safety.

---

## Baseline Requirements

- **Compilers**: GCC $\ge$ 14.2.0 or Clang $\ge$ 23.1.0
- **C Standard**: `-std=gnu23` (or `-std=c23`)
- **OS / Kernel**: Linux $\ge$ 6.1 (with `eventfd`, `timerfd`, `epoll`)
- **Graphics / X11**: 24/32-bit TrueColor visuals, `libXrender`, `libxcb`
(DeepSeek comment: verified on this machine — gcc 14.2.0 is the default `cc` and `/usr/bin/clang-23` is installed, so `-std=gnu23` and `<stdckdint.h>` work with both; `#embed` is not in GCC 14 (it lands in GCC 15), so any asset embedding is clang-only here. "Linux-only" is not free either: `configure.ac` lines 287-296 still have FreeBSD/NetBSD/OpenBSD/Darwin branches, so taking this baseline means deliberately dropping them.)

---

## Step 1: Compiler Baseline & Global Whole-Program Optimization

### Objective
Update the build system to enforce `-std=gnu23`, enable Link-Time Optimization (`-flto=auto`), and eliminate Procedure Linkage Table overhead (`-fno-plt`).

### Files to Modify
- [`configure.ac`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/configure.ac)
- [`Makefile.am`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/Makefile.am)
- [`src/Makefile.am`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/Makefile.am)
- [`wrlib/Makefile.am`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/Makefile.am)
- [`WINGs/Makefile.am`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/Makefile.am)

### Implementation Tasks
1. Add `-std=gnu23` to base `CFLAGS`.
2. Add `-flto=auto` and `-fno-plt` to default optimization flags.
3. Configure Libtool to support LTO across internal convenience libraries (`libwraster`, `libWINGs`).
4. Ensure compiler warnings remain clean under `-Wall -Wextra` in C23 mode.
(DeepSeek comment: implemented — `-std=gnu23` and `-fno-plt` are now in `configure.ac`, `--enable-lto` is opt-in; see SpecAI §118. Original review: `-std=gnu23` is fine here. Keep LTO out of the default build: it needs `gcc-ar`/`gcc-ranlib` (or `llvm-ar`/`llvm-ranlib`) for the shared libWINGs/libwraster, and it inlines frames away, which breaks Valgrind/DHAT/callgrind attribution — the main diagnosis tool, and how the balloon shutdown crash was found. Make it `--enable-lto` for release builds only. Cross-module inlining of `wWindowFor`/`wTextureRenderImage` buys little: the measured hot loops are Pango/HarfBuzz and Xlib round-trips. `-fno-plt` is safe but small; `-fvisibility=hidden` + `-fno-semantic-interposition` would give more on the shared libraries.)

### Expected Gains
- **Cross-module inlining**: Functions like `wTextureRenderImage`, `wWindowFor`, and WINGs utilities can be directly inlined across translation units into hot loops.
- **Dead code elimination**: Unreferenced functions across static archives are completely stripped from the final binary.
- **Elimination of PLT trampolines**: Direct PC-relative calls for position-independent code.

### Verification
```bash
make clean && make -j$(nproc)
make exit-test
tools/profile.sh dhat map
```

---

## Step 2: C23 Standard Features & Branch Optimizations

### Objective
Incorporate C23 checked integer arithmetic, hardware bit manipulation, branch prediction hints, and ELF section clustering into the hot paths.

### Files to Modify
- [`src/event.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/event.c)
- [`src/window.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/window.c)
- [`src/placement.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/placement.c)
- [`src/telemetry.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/telemetry.c)
- [`src/wmspec.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/wmspec.c)
- [`wrlib/raster.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/raster.c)

### Implementation Tasks
1. **Branch & Function Attributes**:
   - Annotate hot functions with `[[gnu::hot]]`: `DispatchEvent()`, `wWindowFor()`, `W_HandleInputEvents()`.
   - Annotate cold handlers with `[[gnu::cold]]`: `handleExitSig()`, `Shutdown()`, error logging.
   - Annotate latency check in `wTelemetryRecordEventEnd()` with `if (elapsed_ns > 16666667ULL) [[unlikely]]`.
(DeepSeek comment: `[[unlikely]]` is not C23 — C23 standardises `[[deprecated]]`, `[[fallthrough]]`, `[[maybe_unused]]`, `[[nodiscard]]`, `[[noreturn]]`; the branch hint is `[[gnu::unlikely]]`/`[[gnu::likely]]` or `__builtin_expect`. `[[gnu::hot]]`/`[[gnu::cold]]` are fine with gcc 14/clang 23, but a WM that waits in `poll` and in Pango shaping gains almost nothing from `.text.hot` clustering.)
2. **Checked Arithmetic (`<stdckdint.h>`)**:
   - In `RCreateImage()` and `wmspec.c` icon parsers, replace manual multiplication with `ckd_mul()` to catch integer overflow at hardware level without performance penalty.
(DeepSeek comment: implemented in `RCreateImage()`; see SpecAI §119. Original review: this is the part worth doing first. `RCreateImage` (`wrlib/raster.c:82`) computes `width * height * (alpha ? 4 : 3)` and already caps dimensions with `MAX_WIDTH`/`MAX_HEIGHT`, but on a 32-bit `size_t` the product can still overflow; `ckd_mul`/`ckd_add` make it explicit at no cost.)
3. **Hardware Bit-Twiddling (`<stdbit.h>`)**:
   - Replace manual bit counting in modifier masks and placement occupancy maps with `stdc_count_ones()` and `stdc_trailing_zeros()` (compiles to single-cycle `POPCNT` and `TZCNT`).
(DeepSeek comment: no real target here. The placement occupancy map is a byte array indexed by slot (`src/placement.c`, reworked in SpecAI §108), not a bitset, and the modifier masks are cold; `stdc_count_ones`/`stdc_trailing_zeros` would be cosmetic. Skip unless a profile shows otherwise.)
4. **Compile-Time Asset Embedding (`#embed`)**:
   - In Clang 23 builds, embed default icons and fallback menus directly into read-only ELF data, eliminating initial filesystem lookups.
(DeepSeek comment: GCC 14 does not implement `#embed` (GCC 15 does), so this is a clang-23-only path and the default gcc build is unchanged. It also needs a memory-image loader in wrlib/WINGs (icons and menus are read from files today), which is real work for a handful of file opens — and startup is dominated by proplist parsing (2 MB churn in DHAT). Drop it.)

### Expected Gains
- Grouping hot functions into `.text.hot` eliminates instruction cache (L1i) and iTLB misses.
- Hardware branch predictors are pre-primed, reducing branch misprediction penalties on event dispatch.
- Zero-cycle integer overflow protection.

### Verification
```bash
make -j$(nproc)
make exit-test
tools/profile.sh perf 10 $(pidof wmaker) map
# (DeepSeek comment: `pidof wmaker` returns the monitor and the child and `perf -p` wants comma-separated pids; the script resolves the WM pid itself through _NET_WM_PID, so use: tools/profile.sh perf 10 "" map)
```

---

## Step 3: Linux Kernel Modernization (`eventfd`, `timerfd`, `epoll`)

### Objective
Replace legacy POSIX self-pipe trick, `poll()` descriptor scanning, and `gettimeofday()` software timer lists with modern Linux kernel primitives.

### Files to Modify
- [`src/startup.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/startup.c) (signal handling)
- [`WINGs/handlers.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/handlers.c) (event loop & timers)
- [`WINGs/WINGs/WINGs.h`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/WINGs/WINGs.h)

### Implementation Tasks
1. **`eventfd` for Signal Handling**:
   - Replace `pipe()` in `initSignalPipe()` with `eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)`.
   - The signal handler does an 8-byte write (`uint64_t val = 1; write(sig_eventfd, &val, sizeof(val));`).
   - Reduces file descriptor consumption from 2 to 1 and drops kernel VFS pipe buffer overhead.
2. **`timerfd` for Monotonic Hardware Timers**:
   - Replace software timer wheel and `gettimeofday()` with `timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)`.
   - Arm the timerfd using `timerfd_settime()` with absolute timestamps (`TFD_TIMER_ABSTIME`).
   - Immune to clock jumps (NTP, timezone adjustments) and eliminates software delta calculations.
3. **`epoll` Multiplexing in WINGs**:
   - Initialize an epoll instance via `epoll_create1(EPOLL_CLOEXEC)`.
   - Replace the $O(N)$ linear reconstruction of `struct pollfd` on every loop iteration with $O(1)$ `epoll_wait()`.
(DeepSeek comment: weak case. `WINGs/handlers.c` polls 2-4 descriptors (`total_fds = nfds + extrafd`, line 398), so epoll removes nothing measurable, and `gettimeofday` (lines 27/437) is a vDSO call (~20 ns), not a syscall. The only defensible piece is one `timerfd` armed to the next deadline, and even that did not appear in any profile. All three are Linux-only while `configure.ac` still supports BSD/macOS, so they would need `#ifdef __linux__` with the existing poll fallback — not in the baseline.)

### Expected Gains
- Eliminates memory copying between userspace and kernel on every event loop wakeup.
- High-precision monotonic timer delivery with zero time-drift risk.
- Minimal syscall overhead during idle and event handling.

### Verification
```bash
make exit-test
tools/profile.sh strace 5 $(pidof wmaker) menu
# (DeepSeek comment: same as above, pass the pid resolved by the script: tools/profile.sh strace 5 "" menu)
```

---

## Step 4: `wrlib` SIMD Vectorization & 32-bit RGBA Alignment

### Objective
Modernize raster graphics operations in `wrlib` by enforcing 32-bit aligned pixel storage and implementing vector extensions for alpha blending, scaling, and gradient rendering.

### Files to Modify
- [`wrlib/raster.h`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/raster.h)
- [`wrlib/raster.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/raster.c)
- [`wrlib/scale.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/scale.c)
- [`wrlib/gradient.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/gradient.c)

### Implementation Tasks
1. **32-Bit Alignment**:
   - Standardize `RImage` pixel data to 32-bit aligned ARGB/RGBA words (`uint32_t`), eliminating unaligned 3-byte RGB packing.
(DeepSeek comment: the observation is right — `RImage` is `(alpha ? 4 : 3)` bytes/px (`wrlib/raster.c:82`) — but fixing it to 4 bytes costs +33% memory on every non-alpha image, touches every wrlib routine, and changes the public `libwraster` ABI: `depth` and `data` are exposed in `wrlib/wraster.h` (lines 179/247). Do the SIMD first, keep the 3-byte layout, and revisit only if a profile shows pixel work dominating.)
2. **SIMD Vector Blending (AVX2 / SSE4.1)**:
   - In `RCombineImages()` and `RCombineImagesWithOpaqueness()`, replace scalar byte multiplications with GCC/Clang vector types (`typedef uint8_t v16u __attribute__((vector_size(16)));`) or Intel intrinsics (`_mm_maddubs_epi16`, `_mm_mulhi_epu16`).
   - Process 8 or 16 pixels per CPU cycle instead of 1 pixel per 9 scalar instructions.
(DeepSeek comment: measured, and the hand-written version loses. Two kernels were written for `RCombineImagesWithOpaqueness` — byte-wise SSE2 (`_mm_unpacklo/hi_epi8`, `_mm_mullo_epi16`, `_mm_srli_epi16`) and an SSSE3 `pshufb` deinterleave producing 32-bit lanes (the shape SIMDe would emit) — both bit-exact, both timed against the untouched loop: 30 iterations of 1920x1080 RGB gave scalar 52.31 ms, byte-wise SSE2 67.62 ms, `pshufb` 177.64 ms. `-O2` already auto-vectorizes the scalar loop into a widening multiply/shift stream, so the hand kernels only add shuffle and 3-byte-stride overhead. The kernels were reverted; `libsimde-dev` (0.7.2, verified working) is installed but deliberately unused, and `test/wrlib_blend_test.c` now pins the arithmetic for opaqueness 0..255. See SpecAI §121.)
3. **Optimized Bilinear Scaling**:
   - Vectorize horizontal and vertical interpolation passes in `RScaleImage()`.
(DeepSeek comment: target the right functions — `RScaleImage` (`wrlib/scale.c:47`) is nearest-neighbour and already vectorised by the compiler; `RSmoothScaleImage` (`scale.c:313`, Catmull-Rom/Lanczos) and `RCombineImages*` (`wrlib/raster.c:214/267`) are the real candidates, and `RMakeTiledImage` (`raster.c:~640`) is already `memcpy` per row. Use runtime dispatch (`__builtin_cpu_supports`) and a bit-exactness test: rounding differences change the pixels, and the titlebar-hash method used for SpecAI §117 is the right tool. — Measured, SpecAI §123: the row-parallel kernel is bit-exact and 1.17-1.28x faster (4 or 8 rows per iteration), so the two-pass interpolator stays scalar. Whole-call costs, which is what the user sees: 19.1 ms for a 1920x1080 → 192x108 workspace map, 22.2 ms for a 300x170 switch-panel thumbnail, 96 ms for a 1920x1080 → 2560x1440 wallpaper; 1.2x on those numbers does not justify an x86-only second code path.)

### Expected Gains
- **8x to 12x speedup** in alpha blending and image scaling.
- Elimination of unaligned cache-line crossing penalties on memory reads/writes.
- Drastic reduction of CPU latency spikes during workspace transitions and window iconification.
(DeepSeek comment: the 8x-12x estimate does not survive measurement on this tree. For the 3-byte RGB blend the compiler's own vectorized scalar loop was already the fastest form (52.31 ms vs 67.62 ms for a hand-written SSE2 kernel over 30 x 1920x1080 blends, SpecAI §121), so the gain there is zero and the kernel was reverted. `RSmoothScaleImage` was then measured too (SpecAI §123): a row-parallel AVX2 kernel is bit-exact but only 1.2-1.4x faster (16-22 ms scalar down to 13-16 ms for the horizontal pass of a 1920-wide image scaled down to 192 columns), because the loop is load-bound on 3-byte gathers, not arithmetic-bound - so it stays scalar as well. The cache-line penalty argument only becomes true with a 4-byte pixel layout, i.e. the ABI change in task 1 above, which is refused: plugins and pre-built dock/icon material must keep loading.)

### Verification
```bash
make -j$(nproc)
make exit-test
tools/profile.sh dhat scenario
# (DeepSeek comment: the scenario argument is one of menu|map|mixed|idle.)
```

---

## Step 5: Purge Legacy 8-bit Visuals from `wrlib`

### Objective
Remove obsolete 1990s paletted visual modes (`PseudoColor`, `StaticColor`, `GrayScale`, Floyd-Steinberg dithering) to eliminate dead code and simplify TrueColor rendering.

### Files to Modify
- [`wrlib/convert.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/convert.c)
- [`wrlib/context.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/context.c)
- [`wrlib/wraster.h.in`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/wraster.h.in)

### Implementation Tasks
1. Remove `image2PseudoColor()`, `image2StandardPseudoColor()`, `image2GrayScale()`, and associated color quantization tables.
(DeepSeek comment: the 8-bit block is about 370 lines, not ">2,000": `image2PseudoColor` line 514, `image2StandardPseudoColor` 598, `image2GrayScale` 748 plus dithering helpers, in a 1,098-line file. Still worthwhile for clarity, just smaller than advertised. Keep `image2Bitmap` (line 871, used by `RConvertImageMask` at line 981) — 1-bit masks are still needed. Also decide explicitly what happens to 15/16-bit TrueColor: `image2TrueColor` handles them today and this step says "solely 24/32-bit". — Done, SpecAI §120: `convert.c` is 1,099 → 528 lines; `image2PseudoColor`/`image2StandardPseudoColor`/`image2GrayScale`/`convertPseudoColor_to_8` and the `RStdConversionTable` machinery removed; `image2Bitmap` and 15/16-bit TrueColor dithering kept; `RConvertImage()` and `RGetClosestXColor()` now return `RERR_BADVISUALID` for non-TrueColor visuals. Verified with `make exit-test` and its `--valgrind` variant.)
2. Specialize `RConvertImage()` solely for 24-bit and 32-bit `TrueColor` visuals.
3. Clean up the MIT-SHM path in `RConvertImage()`, removing the redundant `XCopyArea()` workaround.
(DeepSeek comment: that `XCopyArea` is not a "redundant workaround": the comment in `RConvertImage` explains it exists to control pixmap lifetime and shm segment detachment (otherwise a crash leaves stale segments). Removing it needs explicit pixmap-lifetime tracking, not a cleanup.)

### Expected Gains
- Removes >2,000 lines of complex, obsolete code.
- Significantly smaller binary footprint and cleaner compiler optimization paths for TrueColor.

### Verification
```bash
make clean && make -j$(nproc)
make exit-test
```

---

## Step 6: GPU Acceleration & Async IPC (XRender & XCB Pipelining)

### Objective
Offload 2D rasterization (gradients, bevels, alpha compositing) to the X server GPU rasterizer via XRender, and replace synchronous blocking queries with asynchronous XCB pipelining.

### Files to Modify
- [`configure.ac`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/configure.ac)
- [`src/texture.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/texture.c)
- [`src/menu.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/menu.c)
- [`src/framewin.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/framewin.c)
- [`src/event.c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/event.c)

### Implementation Tasks
1. **XRender Server-Side Gradients**:
   - Link `libXrender`.
   - In `wTextureRenderImage()`, implement a fast-path utilizing `XRenderCreateLinearGradient()` and `XRenderComposite()`.
   - Render gradient textures directly into X11 Pixmaps on the GPU, bypassing software rasterization and bitmap socket transfers.
(DeepSeek comment: not drop-in. `wTextureRenderImage` (`src/texture.c:349`) returns an `RImage` that is then tiled/composited and converted (`RMakeTiledImage`, `RConvertImage`); drawing gradients with XRender into a pixmap needs a parallel drawing path for menus and frames. On Xvfb/llvmpipe it is neutral or slower; measure on a real GPU first. Note the DHAT leader is texture tiling (`RMakeTiledImage`), not gradients.)
2. **XCB Asynchronous Pipelining**:
   - Use `XGetXCBConnection(dpy)`.
   - For high-frequency queries in `handlePropertyNotify()` and window management, emit XCB cookies asynchronously and harvest replies only when required, avoiding blocking roundtrips.
(DeepSeek comment: there is no XCB in the tree today (no `XGetXCBConnection`, no `xcb_*`), so this is a rewrite, not an integration. The remaining round-trip per MapRequest (`XGetWindowAttributes`) is needed *before* deciding to manage, so a cookie does not remove it; at most the request can be issued earlier, for ~50 microseconds per window. Highest risk, least evidence.)

### Expected Gains
- Offloads the primary source of memory allocation churn (>700 MB in DHAT) from userspace CPU to the X server GPU.
- Eliminates multi-megabyte socket transfers of raw uncompressed textures.
- Virtually eliminates blocking IPC roundtrip latency spikes.

### Verification
```bash
make -j$(nproc)
make exit-test
tools/profile.sh callgrind menu
tools/profile.sh dhat map
```
