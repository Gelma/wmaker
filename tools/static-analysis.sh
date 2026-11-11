#!/usr/bin/env bash
#
# Static analysis for the Window Maker tree.
#
#   tools/static-analysis.sh cppcheck    severity summary (the project scan)
#   tools/static-analysis.sh tidy        clang-tidy over wrlib/ (fast, gated)
#   tools/static-analysis.sh tidy-all    clang-tidy over every build directory
#                                        (~1 h with the analyzer checks: opt-in)
#   tools/static-analysis.sh fanalyzer   gcc -fanalyzer over wrlib/ (fast, gated)
#   tools/static-analysis.sh fanalyzer-all
#                                        gcc -fanalyzer over src/ WINGs/ util/
#                                        WPrefs.app/ too (slow, opt-in: ~10 s
#                                        per translation unit)
#   tools/static-analysis.sh semgrep     the platform rule ids over the tree,
#                                        with file:line (needs the registry)
#   tools/static-analysis.sh check       run everything and compare with the
#                                        baseline in tools/static-analysis.txt
#   tools/static-analysis.sh update      rewrite that baseline
#
# Reports land in $OUT_DIR (default: /tmp/wmaker-analysis).  Nothing here
# writes into the source tree.
#
# Notes learned the hard way (see SpecAI.md 124):
#   * clang-tidy has no unversioned symlink on this machine: look for
#     clang-tidy-<N> before giving up;
#   * there is no compile_commands.json under autotools here and bear does
#     not produce one on this tree, so tools/gen-compile-db.py synthesises
#     it from the flags make uses;
#   * cppcheck's "style" output is ~900 findings of constParameter/
#     variableScope noise - the signal is in warning and above, plus the
#     small set of bugprone/clang-analyzer checks that clang-tidy adds.
#
set -euo pipefail

TOP_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
OUT_DIR=${OUT_DIR:-/tmp/wmaker-analysis}
BASELINE=$TOP_DIR/tools/static-analysis.txt

TIDY_CHECKS='-*,clang-analyzer-*,bugprone-*,-bugprone-easily-swappable-parameters,performance-*,portability-*'

die() {
	echo "error: $*" >&2
	exit 1
}

note() {
	echo "--- $*"
}

find_tool() {
	local name=$1 tool

	for tool in "$name" "$name-23" "$name-22" "$name-21" "$name-20" "$name-19" "$name-18"; do
		command -v "$tool" >/dev/null 2>&1 && { echo "$tool"; return 0; }
	done
	return 1
}

cppcheck_summary() {
	local log=$OUT_DIR/cppcheck.log

	command -v cppcheck >/dev/null 2>&1 || die "cppcheck not installed"
	note "cppcheck"
	# Every directory that ships C code: util/, WPrefs.app/ and wmlib/ used to
	# be outside the scan, so the dockapps and the preference panels were never
	# checked by the gate.
	cppcheck -DHAVE_CONFIG_H -I"$TOP_DIR" -I"$TOP_DIR/src" -I"$TOP_DIR/wrlib" \
		-I"$TOP_DIR/WINGs" -I"$TOP_DIR/util" -I"$TOP_DIR/WPrefs.app" \
		--enable=warning,performance,portability,style \
		--inline-suppr --template='{severity}|{file}:{line}|{id}' -q \
		"$TOP_DIR/src" "$TOP_DIR/wrlib" "$TOP_DIR/WINGs" "$TOP_DIR/test" \
		"$TOP_DIR/util" "$TOP_DIR/WPrefs.app" "$TOP_DIR/wmlib" \
		>"$log" 2>&1 || true

	# severity counts, then the ids that are worth looking at
	printf 'cppcheck total %s\n' "$(wc -l <"$log")"
	cut -d'|' -f1 "$log" | sort | uniq -c | awk '{printf "cppcheck %s %d\n", $2, $1}' | sort
	echo "  top ids:"
	cut -d'|' -f3 "$log" | sort | uniq -c | sort -rn | head -8 | sed 's/^/    /'
}

tidy_summary() {
	local tidy db log

	tidy=$(find_tool clang-tidy) || die "clang-tidy not installed"
	command -v python3 >/dev/null 2>&1 || die "python3 not installed"

	note "clang-tidy ($tidy, scope: ${1:-wrlib})"
	python3 "$TOP_DIR/tools/gen-compile-db.py" "$TOP_DIR" "$OUT_DIR/compile_commands.json" >/dev/null
	db=$OUT_DIR/compile_commands.json
	log=$OUT_DIR/clang-tidy.log

	# Analyse exactly the files the database knows about: a plain *.c glob
	# would also hand over sources the build ignores (load_magick.c with
	# ImageMagick disabled), and those only produce parse errors.
	#
	# Scope: the gated pass stays on wrlib (a few minutes).  Analysing all
	# ~190 translation units with the clang-analyzer checks takes about an
	# hour, which nobody would run as a gate: that is the opt-in 'tidy-all'.
	local scope=${1:-wrlib}
	local -a files
	mapfile -t files < <(python3 - "$db" "$scope" <<'EOF'
import json, sys
db, scope = sys.argv[1], sys.argv[2]
for entry in json.load(open(db, encoding="utf-8")):
    if scope == "wrlib" and not entry["directory"].endswith("/wrlib"):
        continue
    print(entry["file"])
EOF
)

	# The database now covers every build directory and each entry carries its
	# own "directory", so run from the top instead of wrlib/.
	#
	# Feed the files in chunks: clang-tidy has been seen to die with a bus error
	# on a single translation unit, and one invocation for the whole tree would
	# lose every result after the crash.  A chunk that fails is reported and the
	# run continues.
	: >"$log"
	local -i chunk=25 i n=${#files[@]} failed=0
	for ((i = 0; i < n; i += chunk)); do
		if ! ( cd "$TOP_DIR" && "$tidy" -p "$db" --quiet \
			--checks="$TIDY_CHECKS" "${files[@]:i:chunk}" >>"$log" 2>&1 ); then
			failed=$((failed + 1))
		fi
	done
	((failed == 0)) || echo "tidy chunks-failed $failed" >&2

	local total
	total=$(grep -cE ": warning: " "$log" || true)
	printf 'tidy total %d\n' "$total"
	# '|| true' on the whole pipeline: with no warning at all the first grep
	# exits 1 and pipefail would abort the script.
	{ grep -E ": warning: " "$log" || true; } | grep -oE '\[[A-Za-z0-9.,-]+\]$' | tr -d '[]' |
		sort | uniq -c | awk '{printf "tidy %s %d\n", $2, $1}' | sort
	local broken
	broken=$(grep -cE ": error: " "$log" || true)
	((broken == 0)) || echo "tidy parse-errors $broken" >&2
}

fanalyzer_summary() {
	local log=$OUT_DIR/fanalyzer.log

	note "gcc -fanalyzer (wrlib)"
	: >"$log"
	local f
	for f in "$TOP_DIR"/wrlib/*.c; do
		gcc -DHAVE_CONFIG_H -I"$TOP_DIR/wrlib" -I"$TOP_DIR" -DDEBUG \
			-D_XOPEN_SOURCE=700 -D_GNU_SOURCE -std=gnu23 -O0 -fanalyzer \
			-c "$f" -o /dev/null >>"$log" 2>&1 || true
	done

	{ grep -oE '\[-Wanalyzer-[a-z-]+\]' "$log" || true; } | tr -d '[]' | sort | uniq -c |
		awk '{printf "fanalyzer %s %d\n", $2, $1}' | sort
	printf 'fanalyzer total %s\n' "$(grep -c 'warning:' "$log" || true)"
}

# Opt-in extension of the -fanalyzer pass to the rest of the tree.  Not part
# of 'check': it costs roughly 10 s per translation unit (~30 min), while the
# gated wrlib pass takes a couple of minutes.  The flags come from the same
# compile database clang-tidy uses, so they are the real build flags.
fanalyzer_all_summary() {
	local db=$OUT_DIR/compile_commands.json
	local log=$OUT_DIR/fanalyzer-all.log

	command -v python3 >/dev/null 2>&1 || die "python3 not installed"
	note "gcc -fanalyzer (src, WINGs, util, WPrefs.app, wmlib)"
	python3 "$TOP_DIR/tools/gen-compile-db.py" "$TOP_DIR" "$db" >/dev/null
	: >"$log"

	python3 - "$db" "$log" <<'PYEOF'
import json, subprocess, sys

db, log = sys.argv[1], sys.argv[2]
entries = json.load(open(db, encoding="utf-8"))
with open(log, "a", encoding="utf-8") as out:
    for e in entries:
        if "/wrlib/" in e["directory"]:
            continue          # already covered by the gated pass
        cmd = e["command"].replace("clang ", "gcc -O0 -fanalyzer ", 1)
        try:
            p = subprocess.run(cmd, shell=True, cwd=e["directory"],
                               capture_output=True, text=True, timeout=600)
        except subprocess.TimeoutExpired:
            out.write(f"TIMEOUT {e['directory']}/{e['file']}\n")
            continue
        out.write(p.stderr)
PYEOF

	{ grep -oE '\[-Wanalyzer-[a-z-]+\]' "$log" || true; } | tr -d '[]' | sort | uniq -c |
		awk '{printf "fanalyzer-all %s %d\n", $2, $1}' | sort
	printf 'fanalyzer-all total %s\n' "$(grep -c 'warning:' "$log" || true)"
	echo "  (see $log for the file:line of each finding)"
}

semgrep_summary() {
	local sg rules_file=$TOP_DIR/tools/semgrep-rules.txt
	local log=$OUT_DIR/semgrep.log json=$OUT_DIR/semgrep.json
	local -a rules=() args=()
	local r

	sg=$(find_tool semgrep) || die "semgrep not installed"
	command -v python3 >/dev/null 2>&1 || die "python3 not installed"
	[[ -f $rules_file ]] || die "no $rules_file"

	mapfile -t rules < <(sed -e 's/#.*//' -e 's/[[:space:]]*$//' "$rules_file" | grep -v '^$')
	(( ${#rules[@]} > 0 )) || die "$rules_file lists no rule"

	note "semgrep ($sg, ${#rules[@]} rules from $(basename "$rules_file"))"
	for r in "${rules[@]}"; do
		args+=(--config="r/$r")
	done

	# One invocation, not one per rule: the engine is shared and the rule
	# download is the slow part.  A rule that cannot be fetched (Pro rules
	# without a login) makes semgrep exit non-zero and still writes what the
	# other rules found, so the exit status is reported, not honoured.
	"$sg" "${args[@]}" --metrics=off --quiet --json --output "$json" \
		"$TOP_DIR/src" "$TOP_DIR/WINGs" "$TOP_DIR/util" "$TOP_DIR/wrlib" \
		"$TOP_DIR/WPrefs.app" "$TOP_DIR/test" "$TOP_DIR/wmlib" >"$log" 2>&1 || {
			echo "  semgrep exited non-zero, see $log" >&2
		}

	python3 - "$json" "$TOP_DIR" <<'PYEOF'
import collections
import json
import os
import sys

path = sys.argv[1]
top = sys.argv[2]
if not os.path.exists(path):
    print("semgrep total 0")
    raise SystemExit(0)

try:
    with open(path, encoding="utf-8") as f:
        results = json.load(f).get("results", [])
except Exception as exc:  # noqa: BLE001 - a broken file is a result too
    print("semgrep total 0")
    print(f"  {path} unreadable: {exc}")
    raise SystemExit(0)

print(f"semgrep total {len(results)}")
for rule, n in collections.Counter(r["check_id"] for r in results).most_common():
    print(f"semgrep {rule.split('.')[-1]} {n}")

if results:
    def severity(r):
        return r.get("extra", {}).get("metadata", {}).get("confidence", "LOW")

    print("  findings:")
    for r in sorted(results, key=lambda x: (severity(x), x["path"], x["start"]["line"])):
        where = os.path.relpath(r["path"], top)
        print(f"    {where}:{r['start']['line']}  {r['check_id'].split('.')[-1]}")
PYEOF
}

run_all() {
	mkdir -p "$OUT_DIR"
	cppcheck_summary >"$OUT_DIR/counts.new"
	tidy_summary >>"$OUT_DIR/counts.new"
	fanalyzer_summary >>"$OUT_DIR/counts.new"
	grep -v -e '^--- ' -e '^  top ids:$' -e '^    ' "$OUT_DIR/counts.new" >"$OUT_DIR/counts.clean"
	mv "$OUT_DIR/counts.clean" "$OUT_DIR/counts.new"
	cat "$OUT_DIR/counts.new"
}

case ${1:-check} in
cppcheck)
	mkdir -p "$OUT_DIR"
	cppcheck_summary
	;;
tidy)
	mkdir -p "$OUT_DIR"
	tidy_summary
	;;
tidy-all)
	mkdir -p "$OUT_DIR"
	tidy_summary all
	;;
fanalyzer)
	mkdir -p "$OUT_DIR"
	fanalyzer_summary
	;;
fanalyzer-all)
	mkdir -p "$OUT_DIR"
	fanalyzer_all_summary
	;;
semgrep)
	mkdir -p "$OUT_DIR"
	semgrep_summary
	;;
check)
	run_all >/dev/null
	[[ -f $BASELINE ]] || die "no baseline: run 'tools/static-analysis.sh update' first"
	if diff -u "$BASELINE" "$OUT_DIR/counts.new" >"$OUT_DIR/baseline.diff"; then
		echo "static analysis: no regression against $(basename "$BASELINE")"
	else
		echo "static analysis: counts changed (see $OUT_DIR/baseline.diff)" >&2
		cat "$OUT_DIR/baseline.diff" >&2
		exit 1
	fi
	;;
update)
	run_all >/dev/null
	cp "$OUT_DIR/counts.new" "$BASELINE"
	echo "baseline written to $BASELINE"
	;;
*)
	sed -n '3,12p' "$0" | sed 's/^# \{0,1\}//'
	exit 1
	;;
esac
