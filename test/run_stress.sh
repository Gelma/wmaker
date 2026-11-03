#!/usr/bin/env bash
set -euo pipefail

# Default display :0 if not set
export DISPLAY="${DISPLAY:-:0}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$SCRIPT_DIR/wm_stress"

if [[ ! -x "$BIN" ]]; then
    echo "Building wm_stress..."
    make -C "$SCRIPT_DIR" wm_stress
fi

exec "$BIN" "$@"
