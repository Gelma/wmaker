#!/usr/bin/env python3
"""Build a compile_commands.json for wrlib without bear.

bear 3.1.3 does not work on this tree (it either forces an automake refresh
that dies in 'missing aclocal-1.16', or swallows libtool's dependency step
and breaks the build), so the database is synthesised from the flags make
actually uses.

The file list comes from the Makefile's own object list, so files that are
present but not built (load_magick.c with ImageMagick disabled, for example)
are not handed to clang-tidy, which would fail to parse them.

Usage: gen-compile-db.py [srcdir] [output.json]
"""
import json
import os
import re
import sys

root = sys.argv[1] if len(sys.argv) > 1 else "."
out = sys.argv[2] if len(sys.argv) > 2 else "/tmp/wmaker-analysis/compile_commands.json"

wdir = os.path.join(root, "wrlib")
makefile = os.path.join(wdir, "Makefile")

# Flags taken from the libtool compile line for this tree.  Keep in sync with
# configure.ac if the baseline changes (-std=gnu23 comes from there).
flags = [
    "clang", "-DHAVE_CONFIG_H", "-I.", "-I..",
    "-DDEBUG", "-D_XOPEN_SOURCE=700", "-D_GNU_SOURCE",
    "-march=native", "-O2", "-g", "-std=gnu23",
]


def object_list(path):
    """Names from 'am_libwraster_la_OBJECTS = a.lo b.lo ...' (continuations)."""
    if not os.path.exists(path):
        return []
    names, collecting = [], False
    for line in open(path, encoding="utf-8", errors="replace"):
        if line.startswith("am_libwraster_la_OBJECTS"):
            collecting = True
            line = line.split("=", 1)[1]
        if not collecting:
            continue
        line = line.rstrip("\n")
        cont = line.endswith("\\")
        names.extend(line.rstrip("\\").split())
        if not cont:
            break
    return names


names = object_list(makefile)
if names:
    sources = [re.sub(r"\.lo$", ".c", n) for n in names if n.endswith(".lo")]
    sources = [s for s in sources if os.path.exists(os.path.join(wdir, s))]
else:
    sources = sorted(f for f in os.listdir(wdir) if f.endswith(".c"))

entries = [{
    "directory": os.path.abspath(wdir),
    "file": name,
    "arguments": flags + ["-c", name, "-o", "/dev/null"],
} for name in sources]

os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
with open(out, "w", encoding="utf-8") as f:
    json.dump(entries, f, indent=1)
print(f"{len(entries)} entries -> {out}")
