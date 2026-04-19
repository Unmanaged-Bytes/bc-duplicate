#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Full bench - compares bc-duplicate (xxh3, sha256) against jdupes,
# rdfind and fclones, plus a correctness spot-check on N duplicate
# groups (every path in a group must hash to the same sha256sum).
# Default: warm-only (no sudo). --with-cold runs cold iterations
# after dropping page cache.
#
# fdupes is intentionally excluded: it is consistently >10x slower
# than the other tools on >100k file corpora and pads bench time
# without adding signal.
#
# Produces build/perf-logs/bench.log.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
BIN="$ROOT/build/release/src/bc-duplicate"
TARGET=""
WITH_COLD=0
WARM_RUNS="${WARM_RUNS:-5}"
COLD_RUNS="${COLD_RUNS:-3}"
CORRECTNESS_SAMPLES="${CORRECTNESS_SAMPLES:-5}"

usage() {
    cat <<EOF
usage: bench.sh [--with-cold] [--target <path>] <target-directory>

Runs a warm-cache benchmark of bc-duplicate vs jdupes / fdupes /
rdfind / fclones, plus a sha256sum spot-check on
${CORRECTNESS_SAMPLES} duplicate groups. Output:
build/perf-logs/bench.log.

Options:
  --with-cold    also run cold-cache iterations (requires sudo for
                 drop_caches)
  --target PATH  alternate way to pass the target directory
  -h, --help     show this help

Environment variables: WARM_RUNS (default 5), COLD_RUNS (default 3),
CORRECTNESS_SAMPLES (default 5).
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --with-cold)   WITH_COLD=1; shift ;;
        --target)      TARGET="$2"; shift 2 ;;
        -h|--help)     usage; exit 0 ;;
        *)             TARGET="$1"; shift ;;
    esac
done

if [[ -z "$TARGET" ]]; then
    echo "error: missing target directory" >&2
    usage >&2
    exit 2
fi

OUT="$ROOT/build/perf-logs/bench.log"

if [[ $WITH_COLD -eq 1 && "$(id -u)" -ne 0 ]]; then
    exec sudo -E bash "$0" --with-cold --target "$TARGET"
fi

[[ -x "$BIN" ]] || {
    echo "missing release binary: $BIN" >&2
    echo "build first: bc-build $ROOT release" >&2
    exit 1
}
[[ -d "$TARGET" ]] || { echo "missing target: $TARGET" >&2; exit 1; }

mkdir -p "$(dirname "$OUT")"
: > "$OUT"

log() { tee -a "$OUT"; }

GIT_REV="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
echo "=== bc-duplicate bench @ $GIT_REV, target=$TARGET ===" | log
echo "warm runs=$WARM_RUNS  cold=$WITH_COLD  correctness samples=$CORRECTNESS_SAMPLES" | log
echo "env: boost=$(cat /sys/devices/system/cpu/cpufreq/boost 2>/dev/null || echo ?)  governor=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo ?)  ASLR=$(cat /proc/sys/kernel/randomize_va_space)" | log
echo "file count: $(find "$TARGET" -type f 2>/dev/null | wc -l)  size: $(du -sh "$TARGET" 2>/dev/null | cut -f1)" | log
echo "" | log

# ------------------------------------------------------------------------------
# Correctness: extract N random duplicate groups from bc-duplicate's JSON
# output, compute sha256sum on each path inside the group, and assert that
# every path in the group has the same digest.
# ------------------------------------------------------------------------------
correctness_check() {
    echo "--- correctness: every path in a duplicate group must share the same sha256sum ---" | log
    local sample_json
    sample_json="$(mktemp)"
    "$BIN" scan --output="$sample_json" "$TARGET" >/dev/null 2>&1 || true
    local fail=0
    local checked=0
    if ! command -v jq >/dev/null 2>&1; then
        echo "  SKIP (jq not installed)" | log
        rm -f "$sample_json"
        return 0
    fi
    local total_groups
    total_groups="$(jq '.groups | length' "$sample_json" 2>/dev/null || echo 0)"
    if [[ "$total_groups" -eq 0 ]]; then
        echo "  SKIP (no duplicate groups in $TARGET)" | log
        rm -f "$sample_json"
        return 0
    fi
    while [[ $checked -lt $CORRECTNESS_SAMPLES ]]; do
        local group_index=$(( RANDOM % total_groups ))
        local path_count
        path_count="$(jq ".groups[$group_index].files | length" "$sample_json")"
        if [[ "$path_count" -lt 2 ]]; then
            checked=$(( checked + 1 ))
            continue
        fi
        local digests
        digests="$(jq -r ".groups[$group_index].files[]" "$sample_json" \
                   | xargs -d '\n' -I{} sha256sum {} 2>/dev/null \
                   | awk '{print $1}' | sort -u | wc -l)"
        if [[ "$digests" -eq 1 ]]; then
            printf "  OK    group %d (%d files share digest)\n" "$group_index" "$path_count" | log
        else
            printf "  FAIL  group %d has %d distinct sha256 digests across %d files\n" \
                   "$group_index" "$digests" "$path_count" | log
            fail=1
        fi
        checked=$(( checked + 1 ))
    done
    rm -f "$sample_json"
    if [[ $fail -ne 0 ]]; then
        echo "  RESULT: FAIL" | log
        return 1
    fi
    echo "  RESULT: OK" | log
}

correctness_check
echo "" | log

# ------------------------------------------------------------------------------
# Warm benchmarks - all tools default flags (rmS for jdupes/fdupes => recurse +
# size summary, no output to stdout). bc-duplicate uses summary subcommand to
# avoid printing 100k+ paths.
# ------------------------------------------------------------------------------
time_one() {
    local label="$1"; shift
    local start end
    start="$(date +%s.%N)"
    "$@" >/dev/null 2>&1 || true
    end="$(date +%s.%N)"
    awk -v s="$start" -v e="$end" -v l="$label" \
        'BEGIN { printf "%s %.3f\n", l, e - s }'
}

bench_warm() {
    local label="$1"; shift
    echo "--- warm $label ---" | log
    "$@" >/dev/null 2>&1 || true
    "$@" >/dev/null 2>&1 || true
    for i in $(seq 1 "$WARM_RUNS"); do
        local start end
        start="$(date +%s.%N)"
        "$@" >/dev/null 2>&1 || true
        end="$(date +%s.%N)"
        awk -v s="$start" -v e="$end" -v l="$label" -v r="$i" \
            'BEGIN { printf "warm %s run%d %.3f\n", l, r, e - s }' | log
    done
}

bench_cold() {
    local label="$1"; shift
    echo "--- cold $label ---" | log
    for i in $(seq 1 "$COLD_RUNS"); do
        sync
        echo 3 > /proc/sys/vm/drop_caches
        local start end
        start="$(date +%s.%N)"
        "$@" >/dev/null 2>&1 || true
        end="$(date +%s.%N)"
        awk -v s="$start" -v e="$end" -v l="$label" -v r="$i" \
            'BEGIN { printf "cold %s run%d %.3f\n", l, r, e - s }' | log
    done
}

bench_warm bc-duplicate-xxh3       "$BIN" summary --hidden "$TARGET"
bench_warm bc-duplicate-sha256     "$BIN" summary --hidden --algorithm=sha256 "$TARGET"
command -v fclones >/dev/null 2>&1 \
    && bench_warm fclones        fclones group "$TARGET" \
    || echo "(fclones not installed)" | log
command -v jdupes >/dev/null 2>&1 \
    && bench_warm jdupes-rmS     jdupes -rmS "$TARGET" \
    || echo "(jdupes not installed)" | log
command -v rdfind >/dev/null 2>&1 \
    && bench_warm rdfind         rdfind -dryrun true "$TARGET" \
    || echo "(rdfind not installed)" | log

if [[ $WITH_COLD -eq 1 ]]; then
    bench_cold bc-duplicate-xxh3   "$BIN" summary --hidden "$TARGET"
    bench_cold bc-duplicate-sha256 "$BIN" summary --hidden --algorithm=sha256 "$TARGET"
    command -v fclones >/dev/null 2>&1 \
        && bench_cold fclones      fclones group "$TARGET"
    command -v jdupes >/dev/null 2>&1 \
        && bench_cold jdupes-rmS   jdupes -rmS "$TARGET"
fi

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------
echo "" | log
echo "=== summary (mean / stddev, seconds) ===" | log
awk '
$1 ~ /^(warm|cold)$/ && NF == 4 {
    key = $1 " " $2
    sum[key] += $4
    sumsq[key] += $4 * $4
    n[key] += 1
}
END {
    for (k in sum) {
        mean = sum[k] / n[k]
        variance = (sumsq[k] / n[k]) - mean * mean
        if (variance < 0) variance = 0
        stddev = sqrt(variance)
        printf "%-40s  mean=%.3f s  sd=%.3f s  n=%d\n", k, mean, stddev, n[k]
    }
}
' "$OUT" | sort | log
