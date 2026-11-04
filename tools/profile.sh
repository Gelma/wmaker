#!/usr/bin/env bash
#
# Profiling helper for Window Maker.
#
# Three tiers, as described in Suggested_Profile_DeepSeek.md:
#
#   1. live session, low overhead:   profile.sh perf [seconds] [pid]
#                                    profile.sh strace [seconds] [pid]
#   2. allocation profile:           profile.sh heaptrack [scenario]
#   3. scoped deep dive (Xvfb):      profile.sh callgrind [scenario]
#                                    profile.sh massif [scenario]
#                                    profile.sh dhat [scenario]
#
# Scenarios: menu (open/close the root menu), map (wm_stress window churn),
# mixed (both) and idle.  Deep dives always start a private Xvfb display with
# a throw-away HOME, so they never touch the caller's session.  perf/strace
# attach to the running WM identified through _NET_SUPPORTING_WM_CHECK and
# _NET_WM_PID, never by process name (the monitor and other sessions share it).
#
# Options through the environment:
#   WM_BIN, WM_LIB_DIRS, STRESS_BIN, PERF_BIN,
#   WM_PROFILE_SECONDS (default 5), WM_PROFILE_WAIT (default 60),
#   WM_PROFILE_OUT (default /tmp/wm-profile-<timestamp>),
#   WM_PROFILE_DISPLAY (force a display instead of picking a free one),
#   MENU_OPENS (default 3)
#
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
TOP_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)

WM_BIN=${WM_BIN:-$TOP_DIR/src/.libs/wmaker}
WM_LIB_DIRS=${WM_LIB_DIRS:-$TOP_DIR/src/.libs:$TOP_DIR/WINGs/.libs:$TOP_DIR/wrlib/.libs}
STRESS_BIN=${STRESS_BIN:-$TOP_DIR/test/wm_stress}
SECONDS_TO_RUN=${WM_PROFILE_SECONDS:-5}
WAIT_TIMEOUT=${WM_PROFILE_WAIT:-60}
MENU_OPENS=${MENU_OPENS:-3}
OUTDIR=${WM_PROFILE_OUT:-/tmp/wm-profile-$(date +%Y%m%d_%H%M%S)}

XVFB_PID=""
WM_PID=""
TOOL_PID=""

usage() {
	cat <<'EOF'
Usage: profile.sh <command> [arguments]

Commands:
  perf [seconds] [pid] [scenario]
                            perf stat + perf record -g on the running WM;
                            scenario (menu/mixed) generates load during the run
  strace [seconds] [pid] [scenario]
                            syscall census of the running WM (raw trace plus
                            a per-syscall summary)
  heaptrack [scenario]      allocation profile of a private instance
  callgrind [scenario]      scoped CPU profile (startup not instrumented)
  massif [scenario]         memory profile including mmap (--pages-as-heap)
  dhat [scenario]           allocation profile per call site
  help                      this text

Scenarios (default: menu):
  menu    open and close the root menu MENU_OPENS times
  map     run wm_stress (needs "make -C test wm_stress")
  mixed   menu and map
  idle    just wait

Examples:
  tools/profile.sh perf 30
  tools/profile.sh perf 30 1234
  tools/profile.sh callgrind menu
  tools/profile.sh dhat map
EOF
}

die() {
	echo "error: $*" >&2
	exit 1
}

note() {
	echo "--- $*" >&2
}

cleanup() {
	if [[ -n "$WM_PID" ]] && kill -0 "$WM_PID" 2>/dev/null; then
		kill -TERM "$WM_PID" 2>/dev/null || true
	fi
	if [[ -n "$XVFB_PID" ]] && kill -0 "$XVFB_PID" 2>/dev/null; then
		kill "$XVFB_PID" 2>/dev/null || true
	fi
}

trap cleanup EXIT

# --- helpers -----------------------------------------------------------------

find_perf() {
	if [[ -n "${PERF_BIN:-}" ]]; then
		echo "$PERF_BIN"
		return
	fi
	local candidate
	candidate=$(ls -1 /usr/lib/linux-tools/*/perf 2>/dev/null | sort -V | tail -1 || true)
	if [[ -n "$candidate" && -x "$candidate" ]] && "$candidate" stat -e cycles true >/dev/null 2>&1; then
		echo "$candidate"
		return
	fi
	echo "perf"
}

check_window() {
	xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null |
		sed -n 's/.*window id # \(0x[0-9a-f]*\).*/\1/p'
}

wm_pid_from_x() {
	local check pid

	check=$(check_window)
	[[ -n "$check" ]] || return 1
	pid=$(xprop -id "$check" _NET_WM_PID 2>/dev/null |
		sed -n 's/.*= *\([0-9][0-9]*\).*/\1/p')
	[[ -n "$pid" ]] || return 1
	echo "$pid"
}

wait_for_wm() {
	local waited=0
	local tool_pid=${1:-}

	while ((waited < WAIT_TIMEOUT * 4)); do
		if wm_pid_from_x >/dev/null 2>&1; then
			WM_PID=$(wm_pid_from_x)
			return 0
		fi
		if [[ -n "$tool_pid" ]] && ! kill -0 "$tool_pid" 2>/dev/null; then
			return 1
		fi
		sleep 0.25
		waited=$((waited + 1))
	done
	return 1
}

pick_display() {
	local n
	for n in $(seq 40 79); do
		if [[ -e "/tmp/.X${n}-lock" || -e "/tmp/.X11-unix/X${n}" ]]; then
			continue
		fi
		echo ":$n"
		return 0
	done
	return 1
}

start_xvfb() {
	local display

	display=${WM_PROFILE_DISPLAY:-}
	if [[ -z "$display" ]]; then
		display=$(pick_display) || die "no free X display in :40-:79"
	fi
	Xvfb "$display" -screen 0 1024x768x24 >"$OUTDIR/xvfb.log" 2>&1 &
	XVFB_PID=$!
	sleep 2
	kill -0 "$XVFB_PID" 2>/dev/null || die "Xvfb failed to start on $display"
	export DISPLAY="$display"
}

start_private_wm() {
	# $1 = log file, rest = command prefix (valgrind/heaptrack options)
	local log=$1
	shift

	mkdir -p "$OUTDIR/home"
	env DISPLAY="$DISPLAY" HOME="$OUTDIR/home" LD_LIBRARY_PATH="$WM_LIB_DIRS" \
		WM_RUNTIME_LOG_DIR="$OUTDIR/runtime" "$@" "$WM_BIN" --for-real \
		>>"$log" 2>&1 </dev/null &
	TOOL_PID=$!
	WM_PID=$TOOL_PID

	if ! wait_for_wm "$TOOL_PID"; then
		echo "window manager log ($log):" >&2
		tail -n 20 "$log" >&2 || true
		die "the window manager did not start"
	fi
}

build_sendkey() {
	local src="$OUTDIR/sendkey.c"
	local bin="$OUTDIR/sendkey"

	if [[ -x "$bin" ]]; then
		echo "$bin"
		return
	fi

	cat >"$src" <<'EOF'
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	Display *dpy = XOpenDisplay(argc > 1 ? argv[1] : NULL);
	KeyCode kc;

	if (!dpy || argc < 3)
		return 1;
	kc = XKeysymToKeycode(dpy, XStringToKeysym(argv[2]));
	if (!kc)
		return 1;
	XTestFakeKeyEvent(dpy, kc, True, CurrentTime);
	XSync(dpy, False);
	XTestFakeKeyEvent(dpy, kc, False, CurrentTime);
	XSync(dpy, False);
	XCloseDisplay(dpy);
	return 0;
}
EOF

	cc -O2 -o "$bin" "$src" -lX11 -lXtst 2>"$OUTDIR/sendkey.log" ||
		die "cannot build the XTest helper (need libXtst headers); use the 'map' scenario instead"
	echo "$bin"
}

run_scenario() {
	local name=${1:-menu}
	local sendkey i

	case "$name" in
	menu)
		sendkey=$(build_sendkey)
		note "scenario: menu ($MENU_OPENS opens)"
		for ((i = 0; i < MENU_OPENS; i++)); do
			"$sendkey" "$DISPLAY" F12
			sleep 0.4
			"$sendkey" "$DISPLAY" Escape
			sleep 0.3
		done
		;;
	map)
		[[ -x "$STRESS_BIN" ]] || die "build the stress tool first: make -C test wm_stress"
		note "scenario: map ($SECONDS_TO_RUN s of wm_stress)"
		timeout "$((SECONDS_TO_RUN + 15))" "$STRESS_BIN" -t 2 -d "$SECONDS_TO_RUN" \
			>/dev/null 2>&1 || true
		;;
	mixed)
		run_scenario menu
		run_scenario map
		;;
	idle)
		note "scenario: idle ($SECONDS_TO_RUN s)"
		sleep "$SECONDS_TO_RUN"
		;;
	*)
		die "unknown scenario: $name (menu, map, mixed, idle)"
		;;
	esac
}

# --- live-session commands ---------------------------------------------------

cmd_perf() {
	local seconds=${1:-30}
	local pid=${2:-}
	local scenario=${3:-}
	local perf load_pid=""

	perf=$(find_perf)
	[[ -n "$pid" ]] || pid=$(wm_pid_from_x || true)
	[[ -n "$pid" ]] || die "cannot find the running WM (set _NET_SUPPORTING_WM_CHECK/_NET_WM_PID or pass a pid)"
	command -v "$perf" >/dev/null 2>&1 || die "perf not found: set PERF_BIN (the /usr/bin wrapper may not match this kernel)"

	start_load() {
		if [[ -n "$scenario" ]]; then
			run_scenario "$scenario" &
			load_pid=$!
		fi
	}

	stop_load() {
		if [[ -n "$load_pid" ]]; then
			wait "$load_pid" 2>/dev/null || true
			load_pid=""
		fi
	}

	start_load
	note "perf stat on pid $pid for ${seconds}s"
	"$perf" stat -p "$pid" \
		-e cycles,instructions,branches,branch-misses,cache-references,cache-misses,page-faults,context-switches \
		-- sleep "$seconds" 2>&1 | tee "$OUTDIR/perf-stat.txt"
	stop_load

	start_load
	note "perf record on pid $pid for ${seconds}s"
	"$perf" record -e cycles:u -F 199 -g -p "$pid" -o "$OUTDIR/perf.data" -- sleep "$seconds" \
		>"$OUTDIR/perf-record.txt" 2>&1 || die "perf record failed (see $OUTDIR/perf-record.txt)"
	stop_load

	"$perf" report -i "$OUTDIR/perf.data" --stdio --sort symbol --no-children \
		>"$OUTDIR/perf-report.txt" 2>&1 || true
	if grep -q "has no samples" "$OUTDIR/perf-report.txt"; then
		note "no user-space samples: the WM was idle. Re-run with a scenario, e.g. 'perf 15 <pid> menu', or use the deep dives on a private display."
	else
		echo
		head -n 30 "$OUTDIR/perf-report.txt"
		echo
	fi
	note "full report: $OUTDIR/perf-report.txt"
}

cmd_strace() {
	local seconds=${1:-30}
	local pid=${2:-}
	local scenario=${3:-}
	local load_pid=""

	[[ -n "$pid" ]] || pid=$(wm_pid_from_x || true)
	[[ -n "$pid" ]] || die "cannot find the running WM (pass a pid)"
	command -v strace >/dev/null 2>&1 || die "strace not installed"

	if [[ -n "$scenario" ]]; then
		run_scenario "$scenario" &
		load_pid=$!
	fi

	note "strace on pid $pid for ${seconds}s"
	# strace -p cannot run a command and does not print -c summaries when it
	# detaches, so collect the raw lines for the window and summarize here.
	timeout -s INT "$seconds" strace -f -p "$pid" -o "$OUTDIR/strace.raw" || true

	if [[ -n "$load_pid" ]]; then
		wait "$load_pid" 2>/dev/null || true
	fi

	awk '{ if ($1 ~ /^\[/) { name = $2 } else if ($1 ~ /^[0-9]+$/) { name = $2 } else { name = $1 }
	       sub(/\(.*/, "", name); c[name]++ }
	     END { for (n in c) printf "%8d %s\n", c[n], n }' \
		"$OUTDIR/strace.raw" 2>/dev/null | sort -rn | head -n 20 | tee "$OUTDIR/strace.txt"
	note "full trace: $OUTDIR/strace.raw"
}

# --- deep dives --------------------------------------------------------------

cmd_heaptrack() {
	local scenario=${1:-menu}

	command -v heaptrack >/dev/null 2>&1 || die "heaptrack not installed"
	mkdir -p "$OUTDIR"
	start_xvfb
	# -r: raw recording, no automatic GUI analysis (heaptrack_gui would abort
	# without a real display anyway); we interpret the data ourselves below.
	start_private_wm "$OUTDIR/wm.log" heaptrack -r -o "$OUTDIR/heaptrack"
	run_scenario "$scenario"

	kill -TERM "$WM_PID" 2>/dev/null || true
	wait "$WM_PID" 2>/dev/null || true
	WM_PID=""

	# heaptrack writes the raw file when its child (the WM) exits
	[[ -n "$TOOL_PID" ]] && wait "$TOOL_PID" 2>/dev/null || true

	local raw report interpret
	raw=$(ls -1 "$OUTDIR"/heaptrack*.raw.zst 2>/dev/null | head -1 || true)
	[[ -n "$raw" ]] || die "heaptrack produced no raw report in $OUTDIR"

	interpret=$(command -v heaptrack_interpret || true)
	[[ -n "$interpret" ]] || interpret=/usr/lib/heaptrack/libexec/heaptrack_interpret
	[[ -x "$interpret" ]] || die "heaptrack_interpret not found"
	command -v zstd >/dev/null 2>&1 || die "zstd is required to interpret the heaptrack data"

	report="$OUTDIR/heaptrack.zst"
	zstd -dc "$raw" | "$interpret" | zstd -c >"$report" ||
		die "failed to interpret $raw"

	note "heaptrack report: $report"
	heaptrack_print "$report" 2>/dev/null | sed -n '1,60p'
}

cmd_callgrind() {
	local scenario=${1:-menu}

	command -v valgrind >/dev/null 2>&1 || die "valgrind not installed"
	mkdir -p "$OUTDIR"
	start_xvfb
	start_private_wm "$OUTDIR/wm.log" \
		valgrind --tool=callgrind --instr-atstart=no \
		--callgrind-out-file="$OUTDIR/callgrind.out"

	note "enabling instrumentation on pid $WM_PID"
	callgrind_control -i on "$WM_PID" >/dev/null 2>&1 || die "callgrind_control failed"
	run_scenario "$scenario"
	callgrind_control -i off "$WM_PID" >/dev/null 2>&1 || true

	kill -TERM "$WM_PID" 2>/dev/null || true
	wait "$WM_PID" 2>/dev/null || true
	WM_PID=""
	sleep 1

	local dump
	dump=$(ls -1t "$OUTDIR"/callgrind.out* 2>/dev/null | head -1 || true)
	[[ -n "$dump" ]] || die "callgrind produced no dump in $OUTDIR"
	note "annotating $dump"
	callgrind_annotate --threshold=90 --auto=no "$dump" 2>/dev/null | sed -n '/PROGRAM TOTALS/,$p' | head -n 40
}

cmd_massif() {
	local scenario=${1:-menu}

	command -v valgrind >/dev/null 2>&1 || die "valgrind not installed"
	mkdir -p "$OUTDIR"
	start_xvfb
	start_private_wm "$OUTDIR/wm.log" \
		valgrind --tool=massif --pages-as-heap=yes \
		--massif-out-file="$OUTDIR/massif.out"

	run_scenario "$scenario"

	kill -TERM "$WM_PID" 2>/dev/null || true
	wait "$WM_PID" 2>/dev/null || true
	WM_PID=""
	sleep 1

	[[ -f "$OUTDIR/massif.out" ]] || die "massif produced no output"
	note "massif: $OUTDIR/massif.out"
	ms_print "$OUTDIR/massif.out" | sed -n '1,50p'
}

cmd_dhat() {
	local scenario=${1:-menu}

	command -v valgrind >/dev/null 2>&1 || die "valgrind not installed"
	mkdir -p "$OUTDIR"
	start_xvfb
	start_private_wm "$OUTDIR/wm.log" \
		valgrind --tool=dhat --dhat-out-file="$OUTDIR/dhat.out"

	run_scenario "$scenario"

	kill -TERM "$WM_PID" 2>/dev/null || true
	wait "$WM_PID" 2>/dev/null || true
	WM_PID=""
	sleep 1

	[[ -f "$OUTDIR/dhat.out" ]] || die "dhat produced no output"
	note "dhat: $OUTDIR/dhat.out"
	python3 - "$OUTDIR/dhat.out" <<'PY'
import json
import re
import sys

data = json.load(open(sys.argv[1]))
frames = data["ftbl"]
sites = data["pps"]


def clean(text):
    text = re.sub(r"^0x[0-9A-Fa-f]+: ", "", text)
    return re.sub(r"\(in [^)]*\)", "", text).strip()


def stack(site, depth=6):
    return " <- ".join(clean(frames[i]) for i in site["fs"][:depth] if i < len(frames))


print("total churn: %.1f MB   live at exit: %.1f MB   peak: %.1f MB" % (
    sum(s["tb"] for s in sites) / 1e6,
    sum(s["eb"] for s in sites) / 1e6,
    sum(s["mb"] for s in sites) / 1e6))
print("\ntop live at exit:")
for site in sorted(sites, key=lambda s: s["eb"], reverse=True)[:10]:
    if site["eb"] == 0:
        break
    print("  %9.1f KB  %s" % (site["eb"] / 1024, stack(site)))
print("\ntop churn:")
for site in sorted(sites, key=lambda s: s["tb"], reverse=True)[:10]:
    print("  %9.1f MB  %s" % (site["tb"] / 1e6, stack(site)))
PY
}

# --- main --------------------------------------------------------------------

command=${1:-help}
[[ $# -gt 0 ]] && shift || true

mkdir -p "$OUTDIR"

case "$command" in
perf)
	cmd_perf "${1:-30}" "${2:-}" "${3:-}"
	;;
strace)
	cmd_strace "${1:-30}" "${2:-}" "${3:-}"
	;;
heaptrack)
	cmd_heaptrack "${1:-menu}"
	;;
callgrind)
	cmd_callgrind "${1:-menu}"
	;;
massif)
	cmd_massif "${1:-menu}"
	;;
dhat)
	cmd_dhat "${1:-menu}"
	;;
help|-h|--help)
	usage
	;;
*)
	usage >&2
	die "unknown command: $command"
	;;
esac

note "artifacts in $OUTDIR"
