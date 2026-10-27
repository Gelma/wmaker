#!/usr/bin/env bash
# stop.sh - Chiusura di Window Maker
set -euo pipefail

echo "Chiusura processi Window Maker..."
if pgrep -x "wmaker" > /dev/null; then
    pkill -TERM -x "wmaker" || true
    echo "Segnale TERM inviato a wmaker."
else
    echo "Nessun processo wmaker in esecuzione."
fi
