#!/usr/bin/env python3
"""Build a compile_commands.json for the whole tree without bear.

bear 3.1.3 does not work on this tree (it either forces an automake refresh
that dies in 'missing aclocal-1.16', or swallows libtool's dependency step
and breaks the build), so the database is synthesised from make itself:

  * the flags come from 'make --eval' printing $(DEFS) $(AM_CPPFLAGS)
    $(CPPFLAGS) $(CFLAGS) fully expanded, per build directory.  CPPFLAGS
    matters: it carries -DDEBUG -D_XOPEN_SOURCE=700 -D_GNU_SOURCE, without
    which most translation units fail to parse;
  * the file list comes from the .deps/*.Plo / *.Po dependency files, i.e.
    exactly the sources the build compiled.  Sources that are present but
    not built (load_magick.c with ImageMagick disabled, WPrefs'
    work-in-progress panels) are therefore not handed to clang-tidy, which
    would fail to parse them.

The command is stored as a raw string in the "command" field: shlex-splitting
it would strip the backslash-escaped quotes of defines such as
-DRESOURCE_PATH=\"...\" and change the macro value.

Requires a configured and built tree.  Usage: gen-compile-db.py [srcdir] [output.json]
"""
import json
import os
import subprocess
import sys

root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")
out = sys.argv[2] if len(sys.argv) > 2 else "/tmp/wmaker-analysis/compile_commands.json"

DIRS = ["wrlib", "src", "WINGs", "util", "WPrefs.app", "wmlib"]

PRINT_RULE = '__wm_printflags: ; @echo "$(DEFS) $(AM_CPPFLAGS) $(CPPFLAGS) $(CFLAGS)"'


def flags_for(subdir):
    workdir = os.path.join(root, subdir)
    try:
        res = subprocess.run(["make", "-C", subdir, "--eval", PRINT_RULE, "__wm_printflags"],
                             cwd=root, capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.SubprocessError) as exc:
        print(f"warning: make --eval in {subdir} failed: {exc}", file=sys.stderr)
        return None
    for line in res.stdout.splitlines():
        if line.startswith("make"):
            continue
        if line.strip():
            return line.strip()
    print(f"warning: no flags printed for {subdir}", file=sys.stderr)
    return None


def sources_for(subdir):
    workdir = os.path.join(root, subdir)
    deps = os.path.join(workdir, ".deps")
    names = set()
    if os.path.isdir(deps):
        for entry in os.listdir(deps):
            if entry.endswith(".Plo"):
                names.add(entry[:-4] + ".c")
            elif entry.endswith(".Po"):
                names.add(entry[:-3] + ".c")
    if not names:
        # No dependency files (e.g. wmlib): fall back to the objects that were
        # actually built, which still excludes sources the build ignores.
        for entry in os.listdir(workdir):
            if entry.endswith(".o"):
                names.add(entry[:-2] + ".c")
            elif entry.endswith(".lo"):
                names.add(entry[:-3] + ".c")
    # .deps can hold stale entries from a previous configuration (e.g.
    # load_magick.c after ImageMagick was disabled): clang-tidy would fail to
    # parse them and abort the whole chunk.  Keep only sources whose object
    # exists in the current build.
    built = []
    for n in sorted(names):
        stem = n[:-2]
        if (os.path.exists(os.path.join(workdir, n))
                and (os.path.exists(os.path.join(workdir, stem + ".lo"))
                     or os.path.exists(os.path.join(workdir, stem + ".o")))):
            built.append(n)
    return built


entries = []
for d in DIRS:
    workdir = os.path.join(root, d)
    if not os.path.isfile(os.path.join(workdir, "Makefile")):
        continue
    flags = flags_for(d)
    sources = sources_for(d)
    if flags is None or not sources:
        print(f"warning: skipping {d} (configured and built?)", file=sys.stderr)
        continue
    for src in sources:
        entries.append({
            "directory": workdir,
            # Absolute: clang-tidy resolves "file" against its own working
            # directory, not against "directory", so a relative name would be
            # looked up in the wrong place when the database spans several
            # build directories.
            "file": os.path.join(workdir, src),
            "command": f"clang -I{workdir} -I{root} {flags} -c {src} -o /dev/null",
        })

os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
with open(out, "w", encoding="utf-8") as f:
    json.dump(entries, f, indent=1)
print(f"{len(entries)} entries -> {out}")
