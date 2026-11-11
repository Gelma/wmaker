#!/usr/bin/env bash
#
# `make check` wrapper around run_exit_test.sh.
#
# The exit/restart/reload regression test needs a built Window Maker, xvfb-run
# (or an explicit WM_TEST_DISPLAY) and xprop.  Under `make check` a missing
# prerequisite must SKIP (automake exit code 77), not fail, so the test suite
# still passes on a machine without an X server.  Run run_exit_test.sh directly
# (or `make exit-test`) when you want a hard failure instead.
#
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
TOP_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)

if [[ -z "${WM_TEST_DISPLAY:-}" ]] && ! command -v xvfb-run >/dev/null 2>&1; then
	echo "SKIP: xvfb-run not available (set WM_TEST_DISPLAY to reuse an X server)"
	exit 77
fi

if ! command -v xprop >/dev/null 2>&1; then
	echo "SKIP: xprop not available"
	exit 77
fi

if [[ ! -x "${WM_BIN:-$TOP_DIR/src/.libs/wmaker}" ]]; then
	echo "SKIP: Window Maker binary not built (${WM_BIN:-$TOP_DIR/src/.libs/wmaker})"
	exit 77
fi

exec "$SCRIPT_DIR/run_exit_test.sh" "$@"
