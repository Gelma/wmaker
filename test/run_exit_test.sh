#!/usr/bin/env bash
#
# Exit/restart/reload regression test for Window Maker.
#
# Starts a throw-away Window Maker instance (the real binary, without the
# session monitor) on an isolated X server and asserts that
#
#   * SIGTERM  makes it exit with status 0,
#   * SIGUSR1  makes it re-exec and come back,
#   * SIGUSR2  reloads the defaults and refreshes the telemetry dump,
#   * with --valgrind, no invalid memory access happens on the way out.
#
# An isolated Xvfb display is always used, so the test never touches the
# developer's live session.  Set WM_TEST_DISPLAY to reuse an existing one.
#
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
TOP_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)

WM_BIN=${WM_BIN:-$TOP_DIR/src/.libs/wmaker}
WM_LIB_DIRS=${WM_LIB_DIRS:-$TOP_DIR/src/.libs:$TOP_DIR/WINGs/.libs:$TOP_DIR/wrlib/.libs}
STARTUP_TIMEOUT=${WM_TEST_TIMEOUT:-60}
EXIT_TIMEOUT=${WM_EXIT_TIMEOUT:-15}

USE_VALGRIND=0
FAILURES=0
TEST_HOME=""
WM_PID=""
LAST_STATUS=0
ORIGINAL_ARGS=("$@")

usage() {
	cat <<'EOF'
Usage: run_exit_test.sh [--valgrind] [--help]

  --valgrind   additionally run the SIGTERM case under Valgrind and fail on
               invalid memory accesses
  --help       show this help

Environment:
  WM_BIN            Window Maker binary (default: <build>/src/.libs/wmaker)
  WM_LIB_DIRS       LD_LIBRARY_PATH for the uninstalled build tree
  WM_TEST_DISPLAY   use this X display instead of starting Xvfb
  WM_TEST_TIMEOUT   seconds to wait for the window manager (default: 60)
  WM_EXIT_TIMEOUT   seconds to wait for the exit after a signal (default: 15)
EOF
}

die() {
	echo "error: $*" >&2
	exit 1
}

note() {
	echo "--- $*"
}

pass() {
	echo "PASS: $*"
}

fail() {
	echo "FAIL: $*" >&2
	FAILURES=$((FAILURES + 1))
}

cleanup() {
	if [[ -n "$WM_PID" ]] && kill -0 "$WM_PID" 2>/dev/null; then
		kill -KILL "$WM_PID" 2>/dev/null || true
		wait "$WM_PID" 2>/dev/null || true
	fi
	if [[ -n "$TEST_HOME" ]]; then
		rm -rf -- "$TEST_HOME"
	fi
}

wm_check_window() {
	xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null |
		sed -n 's/.*window id # \(0x[0-9a-f]*\).*/\1/p'
}

# wait_for_new_wm <previous check window>; an empty argument accepts any window
wait_for_new_wm() {
	local previous=$1
	local waited=0
	local current

	while ((waited < STARTUP_TIMEOUT * 4)); do
		current=$(wm_check_window)
		if [[ -n "$current" && "$current" != "$previous" ]]; then
			return 0
		fi
		kill -0 "$WM_PID" 2>/dev/null || return 1
		sleep 0.25
		waited=$((waited + 1))
	done
	return 1
}

start_wm() {
	local log=$1
	local attempt

	shift

	# No window manager is running here: drop the check window left behind by
	# the previous instance so that a stale value cannot satisfy the wait.
	xprop -root -remove _NET_SUPPORTING_WM_CHECK >/dev/null 2>&1 || true

	for attempt in 1 2; do
		env DISPLAY="$DISPLAY" HOME="$TEST_HOME" LD_LIBRARY_PATH="$WM_LIB_DIRS" \
			WM_RUNTIME_LOG_DIR="$TEST_HOME/logs" "$@" "$WM_BIN" --for-real \
			>>"$log" 2>&1 </dev/null &

		WM_PID=$!
		if wait_for_new_wm ""; then
			return 0
		fi

		if kill -0 "$WM_PID" 2>/dev/null; then
			echo "window manager did not register within ${STARTUP_TIMEOUT}s" >&2
			break
		fi

		# The X server may not have processed the previous instance's
		# disconnect yet: it still owns the WM selection, so the new instance
		# refuses to start.  Give the server a moment and retry once.
		WM_PID=""
		sleep 1
	done

	echo "window manager log ($log):" >&2
	tail -n 20 "$log" >&2 || true
	return 1
}

# stop_wm <signal>: waits for the process to exit and stores the status in
# LAST_STATUS.  A watchdog kills the process if it does not exit in time.
stop_wm() {
	local sig=$1
	local watchdog

	LAST_STATUS=0
	if ! kill -"$sig" "$WM_PID" 2>/dev/null; then
		fail "could not send SIG$sig, window manager is gone"
		WM_PID=""
		return 1
	fi

	( sleep "$EXIT_TIMEOUT"; kill -KILL "$WM_PID" 2>/dev/null ) &
	watchdog=$!

	set +e
	wait "$WM_PID"
	LAST_STATUS=$?
	set -e

	kill "$watchdog" 2>/dev/null || true
	wait "$watchdog" 2>/dev/null || true
	WM_PID=""
}

test_sigterm() {
	local log="$TEST_HOME/exit.log"

	note "SIGTERM exit"
	if ! start_wm "$log"; then
		fail "window manager did not start"
		WM_PID=""
		return
	fi
	if ! stop_wm TERM; then
		return
	fi
	if [[ "$LAST_STATUS" -eq 0 ]]; then
		pass "SIGTERM exit status 0"
	else
		fail "SIGTERM exit status $LAST_STATUS (expected 0)"
	fi
}

test_sigusr1() {
	local log="$TEST_HOME/restart.log"
	local check_window

	note "SIGUSR1 restart"
	if ! start_wm "$log"; then
		fail "window manager did not start"
		WM_PID=""
		return
	fi

	check_window=$(wm_check_window)
	kill -USR1 "$WM_PID"
	sleep 2
	if ! kill -0 "$WM_PID" 2>/dev/null; then
		fail "SIGUSR1 killed the window manager"
		WM_PID=""
		return
	fi
	if ! grep -q "restarting" "$log"; then
		fail "SIGUSR1 did not log a restart"
	fi
	if ! wait_for_new_wm "$check_window"; then
		fail "window manager did not come back after SIGUSR1"
	fi

	if ! stop_wm TERM; then
		return
	fi
	if [[ "$LAST_STATUS" -eq 0 ]]; then
		pass "SIGUSR1 restart, then clean exit"
	else
		fail "exit status after restart: $LAST_STATUS (expected 0)"
	fi
}

test_sigusr2() {
	local log="$TEST_HOME/reload.log"
	local stats="$TEST_HOME/logs/runtime_stats.json"
	local before=""
	local after=""

	note "SIGUSR2 reload and telemetry dump"
	if ! start_wm "$log"; then
		fail "window manager did not start"
		WM_PID=""
		return
	fi

	if [[ -f "$stats" ]]; then
		before=$(stat -c %y "$stats")
	fi

	kill -USR2 "$WM_PID"
	sleep 3

	if ! kill -0 "$WM_PID" 2>/dev/null; then
		fail "SIGUSR2 killed the window manager"
		WM_PID=""
		return
	fi

	if [[ -f "$stats" ]]; then
		after=$(stat -c %y "$stats")
	fi
	if [[ -z "$after" || ( -n "$before" && "$after" == "$before" ) ]]; then
		fail "SIGUSR2 did not refresh $stats"
	else
		pass "SIGUSR2 reloaded defaults and refreshed telemetry"
	fi

	if ! stop_wm TERM; then
		return
	fi
	if [[ "$LAST_STATUS" -ne 0 ]]; then
		fail "exit status after reload: $LAST_STATUS (expected 0)"
	fi
}

test_valgrind() {
	local log="$TEST_HOME/valgrind.log"
	local vglog="$TEST_HOME/valgrind.out"

	note "Valgrind shutdown"
	if ! start_wm "$log" valgrind --quiet --leak-check=no --error-exitcode=99 \
		--log-file="$vglog"; then
		fail "window manager did not start under Valgrind"
		WM_PID=""
		return
	fi

	if ! stop_wm TERM; then
		return
	fi
	if [[ "$LAST_STATUS" -ne 0 ]]; then
		fail "Valgrind reported a faulty shutdown (exit status $LAST_STATUS)"
	fi

	if [[ -f "$vglog" ]] && grep -qE "Invalid read|Invalid write|Invalid free|uninitialised|Process terminating" "$vglog"; then
		fail "Valgrind detected invalid memory accesses during shutdown:"
		grep -E "Invalid read|Invalid write|Invalid free|uninitialised|Process terminating" "$vglog" >&2
	else
		pass "Valgrind: no invalid memory accesses during shutdown"
	fi
}

while (($# > 0)); do
	case "$1" in
	--valgrind)
		USE_VALGRIND=1
		;;
	--help|-h)
		usage
		exit 0
		;;
	*)
		usage >&2
		die "unknown option: $1"
		;;
	esac
	shift
done

[[ -x "$WM_BIN" ]] || die "window manager binary not found: $WM_BIN"
command -v xprop >/dev/null || die "xprop is required"

if [[ -n "${WM_TEST_DISPLAY:-}" ]]; then
	export DISPLAY="$WM_TEST_DISPLAY"
elif [[ -z "${WM_TEST_IN_XVFB:-}" ]]; then
	command -v xvfb-run >/dev/null || die "xvfb-run is required (or set WM_TEST_DISPLAY)"
	export WM_TEST_IN_XVFB=1
	exec xvfb-run -a -s "-screen 0 1024x768x24" "$0" "${ORIGINAL_ARGS[@]}"
fi

TEST_HOME=$(mktemp -d "${TMPDIR:-/tmp}/wmaker-exit-test.XXXXXX")
mkdir -p "$TEST_HOME/logs"
trap cleanup EXIT

test_sigterm
test_sigusr1
test_sigusr2
if ((USE_VALGRIND != 0)); then
	test_valgrind
fi

if ((FAILURES > 0)); then
	echo "$FAILURES test(s) failed" >&2
	exit 1
fi
echo "all exit tests passed"
