#!/usr/bin/env python3
"""Build a compile_commands.json for wrlib from the observed gcc command line.

bear 3.1.3 mangled the libtool dependency-file step on this tree, so the
database is synthesised from the flags make actually used (taken from the
libtool compile line) instead of intercepted.
"""
import json
import os
import sys

root = sys.argv[1] if len(sys.argv) > 1 else "."
out = sys.argv[2] if len(sys.argv) > 2 else "/tmp/analysis/compile_commands.json"

wdir = os.path.join(root, "wrlib")
cc = "clang-23"
flags = [
    cc, "-DHAVE_CONFIG_H", "-I.", "-I..", "-I/opt/wmaker/include",
    "-DDEBUG", "-D_XOPEN_SOURCE=700", "-D_GNU_SOURCE",
    "-march=native", "-O2", "-g",
    "-std=gnu23",
]

entries = []
for name in sorted(os.listdir(wdir)):
    if not name.endswith(".c"):
        continue
    entries.append({
        "directory": os.path.abspath(wdir),
        "file": name,
        "arguments": flags + ["-c", name, "-o", "/dev/null"],
    })

os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "w") as f:
    json.dump(entries, f, indent=1)
print(f"{len(entries)} entries -> {out}")
