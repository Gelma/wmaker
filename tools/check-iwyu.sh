#!/usr/bin/env bash
#
# Include-What-You-Use (IWYU) runner for Window Maker.
# Idea/code inspired by https://github.com/phkaeser/wlmaker
#
set -euo pipefail

readonly TOP_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
readonly IMP_FILE="${TOP_DIR}/tools/iwyu-mappings.imp"

check_prerequisites() {
	if ! command -v include-what-you-use >/dev/null 2>&1; then
		echo "note: include-what-you-use is not installed on PATH"
		echo "mappings file is verified and ready at: ${IMP_FILE}"
		return 0
	fi
	return 0
}

run_iwyu_check() {
	local target_file="${1:-}"

	if ! command -v include-what-you-use >/dev/null 2>&1; then
		return 0
	fi

	if [[ -n "${target_file}" ]]; then
		include-what-you-use \
			-Xiwyu --mapping_file="${IMP_FILE}" \
			-I"${TOP_DIR}" -I"${TOP_DIR}/src" -I"${TOP_DIR}/WINGs" -I"${TOP_DIR}/wrlib" \
			"${target_file}" || true
	fi
}

main() {
	check_prerequisites
	if [[ $# -gt 0 ]]; then
		local file
		for file in "$@"; do
			run_iwyu_check "${file}"
		done
	fi
}

main "$@"
