# Breaking to the Future — Step-by-Step Modernization Plan

This document outlines the step-by-step technical implementation plan for modernizing Window Maker, leveraging modern toolchains (**GCC 14+ / Clang 23+**), the **C23 / GNU23** language standard, modern Linux kernel primitives (kernel 6.x+), and modern X11 extensions (**XRender / XCB**).

Backward compatibility with pre-C99 compilers and legacy 1990s visual architectures is intentionally broken in exchange for maximum performance, minimal CPU cycles, reduced syscall overhead, and robust memory safety.

---

## Baseline Requirements

- **Compilers**: GCC $\ge$ 14.2.0 or Clang $\ge$ 23.1.0
- **C Standard**: `-std=gnu23` (or `-std=c23`)
- **OS / Kernel**: Linux $\ge$ 6.1 (with `eventfd`, `timerfd`, `epoll`)
- **Graphics / X11**: 24/32-bit TrueColor visuals, `libXrender`, `libxcb`

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
2. **Checked Arithmetic (`<stdckdint.h>`)**:
   - In `RCreateImage()` and `wmspec.c` icon parsers, replace manual multiplication with `ckd_mul()` to catch integer overflow at hardware level without performance penalty.
3. **Hardware Bit-Twiddling (`<stdbit.h>`)**:
   - Replace manual bit counting in modifier masks and placement occupancy maps with `stdc_count_ones()` and `stdc_trailing_zeros()` (compiles to single-cycle `POPCNT` and `TZCNT`).
4. **Compile-Time Asset Embedding (`#embed`)**:
   - In Clang 23 builds, embed default icons and fallback menus directly into read-only ELF data, eliminating initial filesystem lookups.

### Expected Gains
- Grouping hot functions into `.text.hot` eliminates instruction cache (L1i) and iTLB misses.
- Hardware branch predictors are pre-primed, reducing branch misprediction penalties on event dispatch.
- Zero-cycle integer overflow protection.

### Verification
```bash
make -j$(nproc)
make exit-test
tools/profile.sh perf 10 $(pidof wmaker) map
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

### Expected Gains
- Eliminates memory copying between userspace and kernel on every event loop wakeup.
- High-precision monotonic timer delivery with zero time-drift risk.
- Minimal syscall overhead during idle and event handling.

### Verification
```bash
make exit-test
tools/profile.sh strace 5 $(pidof wmaker) menu
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
2. **SIMD Vector Blending (AVX2 / SSE4.1)**:
   - In `RCombineImages()` and `RCombineImagesWithOpaqueness()`, replace scalar byte multiplications with GCC/Clang vector types (`typedef uint8_t v16u __attribute__((vector_size(16)));`) or Intel intrinsics (`_mm_maddubs_epi16`, `_mm_mulhi_epu16`).
   - Process 8 or 16 pixels per CPU cycle instead of 1 pixel per 9 scalar instructions.
3. **Optimized Bilinear Scaling**:
   - Vectorize horizontal and vertical interpolation passes in `RScaleImage()`.

### Expected Gains
- **8x to 12x speedup** in alpha blending and image scaling.
- Elimination of unaligned cache-line crossing penalties on memory reads/writes.
- Drastic reduction of CPU latency spikes during workspace transitions and window iconification.

### Verification
```bash
make -j$(nproc)
make exit-test
tools/profile.sh dhat scenario
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
2. Specialize `RConvertImage()` solely for 24-bit and 32-bit `TrueColor` visuals.
3. Clean up the MIT-SHM path in `RConvertImage()`, removing the redundant `XCopyArea()` workaround.

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
2. **XCB Asynchronous Pipelining**:
   - Use `XGetXCBConnection(dpy)`.
   - For high-frequency queries in `handlePropertyNotify()` and window management, emit XCB cookies asynchronously and harvest replies only when required, avoiding blocking roundtrips.

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
