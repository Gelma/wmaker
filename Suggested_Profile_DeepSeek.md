# Profiling tools for Window Maker — reference sheet

Saved 2026-11-04, from the session that validated the toolchain on this machine.
Scope: which tools to use to monitor the window manager in normal use, what data each one
provides, and the workflow that turned out to be practical here.

## What is available on this machine (verified)

| Tool | Status | Notes |
|---|---|---|
| Valgrind 3.28 | all tools available | `memcheck, callgrind, cachegrind, massif, dhat, helgrind, drd` + `callgrind_annotate`, `ms_print`, `vgdb`, `callgrind_control` |
| perf | the `/usr/bin` wrapper is broken | the running kernel is `6.6.161-uksm` and no matching `linux-tools` package is installed. The working binary is `/usr/lib/linux-tools/7.0.0-42-generic/perf` (tested: `cycles:u, instructions:u, cache-misses:u, page-faults:u, context-switches:u`) |
| heaptrack | available | `heaptrack`, `heaptrack_print`, `heaptrack_gui`; test run produced a 362 KB sample with `file:line` stacks |
| strace, ltrace, gdb, vgdb | available | |
| UKSM counters (`/sys/kernel/mm/uksm/*`) and `/proc/<pid>/smaps_rollup` | readable | give Pss/Private_Dirty/AnonHugePages and dedup counters without a profiler |
| bpftrace, flamegraph.pl, kcachegrind/hotspot, sysdig | **missing** | installable, need root (sudo) |

Constraints: `perf_event_paranoid=2` means user-space only (no tracepoints, no `perf trace`, no
sched/block events) and `kptr_restrict=1` hides kernel symbols. For a window manager that is fine:
almost all the work is in user space.

## Which Valgrind mode, and for what

| Tool | Purpose here | Command |
|---|---|---|
| **callgrind** (first choice) | CPU cost per function, with file:line; best for "who burns cycles" on one scenario | `valgrind --tool=callgrind --instr-atstart=no --callgrind-out-file=/tmp/cg.out src/.libs/wmaker --for-real`, then `callgrind_control -i on <pid>`, run the scenario, `-i off`, exit and annotate `/tmp/cg.out.1` with `callgrind_annotate --threshold=95 --auto=no` |
| **cachegrind** | L1/LL hit-miss and branch mispredictions in hot loops (image tiling/scaling in `wrlib`, Pango shaping, menu painting) | `valgrind --tool=cachegrind --branch-sim=yes src/.libs/wmaker --for-real` |
| **massif** | memory over time **including mmap** (the ≥2 MB images that DHAT does not see) | `valgrind --tool=massif --pages-as-heap=yes ...` then `ms_print massif.out.<pid>` |
| **dhat** | allocation churn per call site (already used for the 12.5 h audit) | `valgrind --tool=dhat ...` |
| **memcheck** | correctness, not profiling (already the `make exit-test EXIT_TEST_ARGS=--valgrind` gate) | |
| **helgrind/drd** | **not recommended**: one main loop plus Pango's worker thread produces pure noise | |

Ground rules learned the hard way:

* instrument `src/.libs/wmaker --for-real`, never the libtool wrapper `src/wmaker` (exec drops the
  tracing);
* Valgrind times are inflated 10–20x, so use the built-in telemetry, not Valgrind, for latency;
* `valgrind` `exec`s the program: the pid is the Valgrind pid, `/proc/<pid>/task/<pid>/children` is
  empty;
* the first callgrind dump contains only a header; the measured dump is the `.out.1` file;
* never `--zero` mid-run in this Valgrind build (the counters wrap).

## Other tools worth using

1. **perf** (with the version-specific binary) — best for normal use, ~1 % overhead:
   * `perf stat -p <pid> -e cycles,instructions,branches,branch-misses,cache-misses,page-faults,context-switches -- sleep 60`
   * `perf record -F 199 -g -p <pid> -o /tmp/p.data -- sleep 30 && perf report -i /tmp/p.data --stdio --sort symbol`
   * Example output from a 2-thread `wm_stress` run: `XGetWindowAttributes` 20.9 %
     (`handleMapRequest` → `wManageWindow`), `_XReply` 18.1 %, `WMHandleEvent` 19 %,
     `WMWidthOfString` → `pango_layout_get_pixel_size` 12.8 %, `__wmessage` 11.2 %,
     `animateResize` 10.8 %.
2. **heaptrack** — cheap allocation profiling of a real session (much faster than DHAT):
   `heaptrack -o /tmp/wm src/.libs/wmaker --for-real`, then `heaptrack_print /tmp/wm.zst`
   (or `heaptrack_gui`). Example: 85,184 `xcb` allocations with stacks up to `wManageWindow`
   (`window.c:832`).
3. **strace** — syscall census to validate the savings: `strace -c -f -p <pid> -- sleep 30`;
   used to prove 1 `openat` for 20 spike entries.
4. **gdb + /proc** — threads and stacks (`gdb -p <pid> -ex "thread apply all bt"`), `smaps_rollup`
   for Pss/Private_Dirty/AnonHugePages, `/sys/kernel/mm/uksm/*` for dedup effectiveness.
   Real data on the live WM: Rss 40.5 MB, Pss 17.0 MB, AnonHugePages 0 — `MADV_HUGEPAGE` is not
   producing THP today.
5. **Built-in telemetry** — the zero-cost monitor for normal use: per-event latency (modal time
   excluded), `modal_time_ms`, cache hit rates, `font_caches`, spike log with contextual fields.
   Cheap extensions worth having: `getrusage()` (utime/stime, page faults, context switches),
   `mallinfo2()` (arena vs mmap), `XPending()` as the event backlog.

## Three-tier workflow

| Tier | When | What |
|---|---|---|
| Always on | normal use | telemetry: latency without modal time, cache hits, spike log; plus `rusage`/`mallinfo2`/`XPending` if added |
| Real session, 30–60 s | when something looks wrong | `perf stat` / `perf record -g`, `heaptrack`, `strace -c` on the WM pid |
| Scoped deep dive | before/after an optimization | callgrind (CPU), cachegrind (cache), massif (memory incl. mmap), DHAT (churn) on Xvfb with a scripted scenario (menu, map, title storm, iconify) |

## First measurements collected with these tools (2026-11-04)

* callgrind, three root-menu opens after startup: 1.37 G Ir, dominated by Pango/HarfBuzz/Xft glyph
  rendering; `src/window.c:addSingleKeyGrab` 1.45 %, `src/defaults.c:getKeybind` 0.45 % plus
  `setKeyGrab` 0.35 %, `src/menu.c:paintEntry` 0.25 % — opening a menu replays the keybinding grabs.
* perf, 2-thread stress: numbers above; the per-map `XGetWindowAttributes` round-trip is the first
  CPU target.
* `smaps_rollup`: AnonHugePages stays 0, so the hugepage advice buys nothing for the heap.

## What is missing, and what it would take

* A `perf` built for the running kernel (or `linux-tools` for `6.6.161-uksm`): today the 7.0.0
  binary works but cannot see kernel time.
* `flamegraph.pl` / `stackcollapse-perf.pl` (`flamegraph` package) for flame graphs from
  `perf script`; the aggregation can be scripted in Python without the package.
* `bpftrace` for near-zero-cost live counters (uprobes on `wmalloc`, `sys_enter_openat`), but it
  needs root and is not installed.

All three need root/sudo, hence they are a separate decision from the code work.
