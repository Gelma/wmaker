#!/usr/bin/env bash
# start.sh - Avvio di Window Maker (/opt/wmaker)
set -euo pipefail

WM_PREFIX="/opt/wmaker"
export PATH="${WM_PREFIX}/bin:${PATH}"
export LD_LIBRARY_PATH="${WM_PREFIX}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export XDG_DATA_DIRS="${WM_PREFIX}/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"

if [[ -z "${DISPLAY:-}" ]]; then
    echo "Nessun display X11 rilevato (\$DISPLAY non impostato)."
    echo "Avvio con xinit / x11..."
    exec xinit "${WM_PREFIX}/bin/wmaker" -- :0
else
    echo "Avvio Window Maker su ${DISPLAY}..."
    exec "${WM_PREFIX}/bin/wmaker" "$@"
fi
