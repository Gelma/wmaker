#!/usr/bin/env bash
#
# Build and run GitHub CodeQL C/C++ static analysis for Window Maker in an
# isolated temporary copy of the working tree, saving SARIF and CSV reports
# under ./QLCodeAnalysis/ (or --out-dir).
#
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

CODEQL_BIN="${CODEQL_BIN:-/opt/dev/codeql/codeql}"
OUT_DIR="${OUT_DIR:-${REPO_ROOT}/QLCodeAnalysis}"
SUITE="${CODEQL_SUITE:-codeql/cpp-queries:codeql-suites/cpp-security-and-quality.qls}"
THREADS="${CODEQL_THREADS:-0}"
JOBS="${MAKE_JOBS:-$(nproc)}"
RAM_MB="${CODEQL_RAM:-}"
CC_BIN="${CC_BIN:-/usr/bin/gcc}"

DB_DIR=""
KEEP_DB=0
REUSE_DB=""
KEEP_WORKDIR=0

WORKDIR=""
SARIF_TMP=""
CSV_TMP=""

log() {
	printf '==> %s\n' "$*"
}

die() {
	printf 'ERROR: %s\n' "$*" >&2
	exit 1
}

usage() {
	cat <<EOF
Usage: $(basename -- "${BASH_SOURCE[0]}") [OPTIONS]

Build Window Maker cleanly under the CodeQL C/C++ extractor (in an isolated
temporary copy of the repository so the working tree stays clean) and export
SARIF v2.1.0 + CSV results into QLCodeAnalysis/.

Options:
  --out-dir <dir>    Output directory for results
                     (default: ${REPO_ROOT}/QLCodeAnalysis)
  --suite <suite>    CodeQL query suite or pack
                     (default: ${SUITE})
  --db-dir <dir>     Persist the CodeQL database at <dir> (implies --keep-db)
  --keep-db          Persist the CodeQL database at ${REPO_ROOT}/codeql-db
  --reuse-db <dir>   Skip build/extraction and analyze an existing database
  --threads <N>      CodeQL query evaluation threads, 0 = all cores (default: ${THREADS})
  --jobs <N>         Parallel jobs for 'make -j' (default: ${JOBS})
  --ram <MB>         Optional CodeQL memory budget in MB
  --keep-workdir     Preserve the temporary /tmp/.wmaker-codeql.* build tree
  -h, --help         Show this help message and exit

Environment overrides:
  CODEQL_BIN, OUT_DIR, CODEQL_SUITE, CODEQL_THREADS, MAKE_JOBS, CODEQL_RAM, CC_BIN
EOF
}

cleanup() {
	if [[ -n "$SARIF_TMP" && -e "$SARIF_TMP" ]]; then
		rm -f -- "$SARIF_TMP"
	fi
	if [[ -n "$CSV_TMP" && -e "$CSV_TMP" ]]; then
		rm -f -- "$CSV_TMP"
	fi
	if [[ -n "$WORKDIR" && -d "$WORKDIR" ]]; then
		if [[ "$KEEP_WORKDIR" -eq 1 ]]; then
			log "Temporary workdir preserved at: $WORKDIR"
		else
			rm -rf -- "$WORKDIR"
		fi
	fi
}
trap cleanup EXIT

to_abs_path() {
	local input_path="$1"

	python3 - "$input_path" <<'PYEOF'
import os
import sys

print(os.path.abspath(sys.argv[1]))
PYEOF
}

require_nonneg_int() {
	local flag="$1"
	local value="$2"

	if [[ ! "$value" =~ ^[0-9]+$ ]]; then
		die "${flag} requires a non-negative integer, got: '${value}'"
	fi
}

parse_args() {
	while [[ $# -gt 0 ]]; do
		case "$1" in
		--out-dir)
			[[ $# -ge 2 ]] || die "--out-dir requires an argument"
			OUT_DIR="$2"
			shift 2
			;;
		--suite)
			[[ $# -ge 2 ]] || die "--suite requires an argument"
			SUITE="$2"
			shift 2
			;;
		--db-dir)
			[[ $# -ge 2 ]] || die "--db-dir requires an argument"
			DB_DIR="$2"
			KEEP_DB=1
			shift 2
			;;
		--keep-db)
			KEEP_DB=1
			shift
			;;
		--reuse-db)
			[[ $# -ge 2 ]] || die "--reuse-db requires an argument"
			REUSE_DB="$2"
			shift 2
			;;
		--threads)
			[[ $# -ge 2 ]] || die "--threads requires an argument"
			THREADS="$2"
			shift 2
			;;
		--jobs)
			[[ $# -ge 2 ]] || die "--jobs requires an argument"
			JOBS="$2"
			shift 2
			;;
		--ram)
			[[ $# -ge 2 ]] || die "--ram requires an argument"
			RAM_MB="$2"
			shift 2
			;;
		--keep-workdir)
			KEEP_WORKDIR=1
			shift
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			die "Unknown option: $1 (use --help for usage)"
			;;
		esac
	done

	require_nonneg_int "--threads" "$THREADS"
	require_nonneg_int "--jobs" "$JOBS"
	if (( JOBS < 1 )); then
		die "--jobs must be >= 1, got: '${JOBS}'"
	fi
	if [[ -n "$RAM_MB" ]]; then
		require_nonneg_int "--ram" "$RAM_MB"
		if (( RAM_MB < 1 )); then
			die "--ram must be >= 1 MB, got: '${RAM_MB}'"
		fi
	fi

	if [[ -n "$REUSE_DB" && -n "$DB_DIR" ]]; then
		die "--reuse-db and --db-dir cannot be combined"
	fi
}

preflight() {
	local tool

	[[ -d "$REPO_ROOT/.git" ]] || die "Git repository not found at: $REPO_ROOT"
	[[ -x "$CODEQL_BIN" ]] || die "CodeQL CLI not executable at: $CODEQL_BIN"
	command -v python3 >/dev/null 2>&1 || die "Required tool not found: python3"

	OUT_DIR="$(to_abs_path "$OUT_DIR")"
	if [[ -n "$DB_DIR" ]]; then
		DB_DIR="$(to_abs_path "$DB_DIR")"
	fi

	if [[ -n "$REUSE_DB" ]]; then
		REUSE_DB="$(to_abs_path "$REUSE_DB")"
		[[ -d "$REUSE_DB" ]] || die "Database directory not found: $REUSE_DB"
		[[ -f "$REUSE_DB/codeql-database.yml" ]] || die "Not a CodeQL database: $REUSE_DB"
	else
		for tool in git make autoconf automake libtool autopoint pkg-config; do
			command -v "$tool" >/dev/null 2>&1 || die "Required build tool not found: $tool"
		done
		[[ -x "$CC_BIN" ]] || die "Compiler not executable at: $CC_BIN"
	fi

	log "Verifying CodeQL query suite: $SUITE"
	if ! "$CODEQL_BIN" resolve queries "$SUITE" >/dev/null 2>&1; then
		die "Failed to resolve CodeQL suite/query: $SUITE"
	fi
}

create_database() {
	local db_target="$1"
	local -a codeql_flags=("--threads=${THREADS}")

	if [[ -n "$RAM_MB" ]]; then
		codeql_flags+=("--ram=${RAM_MB}")
	fi

	log "Copying repository working tree to isolated build dir: $WORKDIR"
	cp -af "$REPO_ROOT/." "$WORKDIR/"

	(
		cd -- "$WORKDIR"
		[[ -d .git && "$PWD" != "$REPO_ROOT" ]] || die "Safety check failed in $WORKDIR"

		log "Cleaning copied tree (git clean -xdf)..."
		git clean -xdf >/dev/null

		log "Running ./autogen.sh..."
		./autogen.sh

		log "Configuring Window Maker cleanly with CC=${CC_BIN} (CCACHE_DISABLE=1)..."
		CC="$CC_BIN" CCACHE_DISABLE=1 ./configure \
			--prefix=/opt/wmaker \
			--sysconfdir=/opt/wmaker/etc \
			--with-pkgconfdir=/opt/wmaker/etc/WindowMaker \
			--enable-debug \
			--enable-modelock \
			--enable-xinerama \
			--enable-randr \
			--enable-usermenu \
			--enable-sni \
			"CC=${CC_BIN}" \
			'CFLAGS=-march=native -O2 -g -Wall'

		mkdir -p -- "$(dirname -- "$db_target")"
		log "Creating CodeQL C/C++ database at $db_target (make -j${JOBS})..."
		CC="$CC_BIN" CCACHE_DISABLE=1 \
			"$CODEQL_BIN" database create "$db_target" \
			--language=cpp \
			--source-root="$WORKDIR" \
			--overwrite \
			"${codeql_flags[@]}" \
			--command="make -j${JOBS}"
	)
}

analyze_and_export() {
	local db_target="$1"
	local sarif_out="${OUT_DIR}/wmaker-results.sarif"
	local csv_out="${OUT_DIR}/wmaker-results.csv"
	local -a analyze_flags=("--threads=${THREADS}")

	if [[ -n "$RAM_MB" ]]; then
		analyze_flags+=("--ram=${RAM_MB}")
	fi

	mkdir -p -- "$OUT_DIR"
	SARIF_TMP="${OUT_DIR}/.wmaker-results.sarif.tmp.$$"
	CSV_TMP="${OUT_DIR}/.wmaker-results.csv.tmp.$$"

	log "Running CodeQL analysis ($SUITE) -> SARIF..."
	"$CODEQL_BIN" database analyze "$db_target" "$SUITE" \
		--format=sarif-latest \
		--output="$SARIF_TMP" \
		"${analyze_flags[@]}"

	log "Exporting computed results -> CSV (without re-running queries)..."
	"$CODEQL_BIN" database interpret-results "$db_target" "$SUITE" \
		--format=csv \
		--output="$CSV_TMP" \
		--threads="$THREADS"

	mv -f -- "$SARIF_TMP" "$sarif_out"
	SARIF_TMP=""
	mv -f -- "$CSV_TMP" "$csv_out"
	CSV_TMP=""

	log "Saved SARIF: $sarif_out ($(du -h -- "$sarif_out" | cut -f1))"
	log "Saved CSV:   $csv_out ($(du -h -- "$csv_out" | cut -f1))"
}

print_summary() {
	local csv_file="${OUT_DIR}/wmaker-results.csv"

	[[ -f "$csv_file" ]] || return 0
	log "Summary from $csv_file:"
	python3 - "$csv_file" <<'PYEOF'
import collections
import csv
import sys

csv_path = sys.argv[1]
by_severity = collections.Counter()
by_component = collections.Counter()
by_rule = collections.Counter()

with open(csv_path, newline="", encoding="utf-8") as fh:
    for row in csv.reader(fh):
        if len(row) < 5:
            continue
        rule, _desc, severity, _msg, path = row[:5]
        by_rule[rule] += 1
        by_severity[severity] += 1
        parts = [p for p in path.split("/") if p]
        component = parts[0] if len(parts) > 1 else "(root)"
        by_component[component] += 1

total = sum(by_severity.values())
print(f"  Total findings: {total}")
for sev in ("error", "warning", "recommendation"):
    print(f"    {sev:15s}: {by_severity.get(sev, 0)}")
for sev, count in sorted(by_severity.items()):
    if sev not in ("error", "warning", "recommendation"):
        print(f"    {sev:15s}: {count}")

if by_component:
    print("  By component:")
    for comp, count in sorted(by_component.items(), key=lambda kv: (-kv[1], kv[0])):
        print(f"    {comp:15s}: {count}")

if by_rule:
    print("  Top 10 rules:")
    for rule, count in by_rule.most_common(10):
        print(f"    {count:4d}  {rule}")
PYEOF
}

main() {
	local db_target

	parse_args "$@"
	preflight

	if [[ -n "$REUSE_DB" ]]; then
		db_target="$REUSE_DB"
		log "Reusing existing CodeQL database: $db_target"
	else
		WORKDIR="$(mktemp -d /tmp/.wmaker-codeql.XXXXXXXX)"
		if [[ -n "$DB_DIR" ]]; then
			db_target="$DB_DIR"
		elif [[ "$KEEP_DB" -eq 1 ]]; then
			db_target="${REPO_ROOT}/codeql-db"
		else
			db_target="${WORKDIR}/codeql-db"
		fi

		create_database "$db_target"
	fi

	analyze_and_export "$db_target"
	print_summary

	if [[ "$KEEP_DB" -eq 1 && -z "$REUSE_DB" ]]; then
		log "CodeQL database kept at: $db_target"
	fi
}

main "$@"
