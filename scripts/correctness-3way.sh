#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# 3-way correctness comparison: bc-duplicate vs fclones vs jdupes.
# Each tool is run on the same target, its output is parsed into one
# canonical line per duplicate group (paths sorted, joined by NUL),
# then we diff the three set-of-lines files.
#
# Output: build/perf-logs/correctness-3way.log
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
BIN="$ROOT/build/release/src/bc-duplicate"
TARGET=""

usage() {
    cat <<EOF
usage: correctness-3way.sh <target-directory>
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        *)         TARGET="$1"; shift ;;
    esac
done

[[ -z "$TARGET" ]] && { usage >&2; exit 2; }
[[ -x "$BIN" ]] || { echo "missing release binary: $BIN" >&2; exit 1; }
[[ -d "$TARGET" ]] || { echo "missing target: $TARGET" >&2; exit 1; }
command -v jq      >/dev/null || { echo "missing jq" >&2; exit 1; }
command -v fclones >/dev/null || { echo "missing fclones" >&2; exit 1; }
command -v jdupes  >/dev/null || { echo "missing jdupes" >&2; exit 1; }

OUT="$ROOT/build/perf-logs/correctness-3way.log"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$(dirname "$OUT")"
: > "$OUT"

log() { tee -a "$OUT"; }

echo "=== 3-way correctness @ $(git -C "$ROOT" rev-parse --short HEAD), target=$TARGET ===" | log
echo "file count: $(find "$TARGET" -type f 2>/dev/null | wc -l)" | log
echo "" | log

# ---- run each tool ----
echo "Running bc-duplicate scan --hidden ..." | log
"$BIN" scan --hidden --output="$WORK/bcd.json" "$TARGET" >/dev/null 2>"$WORK/bcd.stderr" || true

echo "Running fclones group --hidden --no-ignore ..." | log
fclones group --hidden --no-ignore --threads 8 "$TARGET" >"$WORK/fclones.txt" 2>/dev/null || true

echo "Running jdupes -r ..." | log
jdupes -r "$TARGET" >"$WORK/jdupes.txt" 2>/dev/null || true

# ---- canonicalise: each group becomes one line, paths sorted, NUL-joined ----
# Each tool's parser emits one TSV line per group with paths sorted
# (so two tools that find the same group emit the same line); a
# trailing LC_ALL=C sort orders the lines for diff.

jq -r '.groups[] | .files | sort | @tsv' "$WORK/bcd.json" 2>/dev/null \
    | LC_ALL=C sort > "$WORK/bcd-groups.txt"

awk '
function emit(   i, n_sorted, line) {
    if (n < 2) { n = 0; return }
    n_sorted = asort(paths)
    line = paths[1]
    for (i = 2; i <= n_sorted; i++) line = line "\t" paths[i]
    print line
    n = 0
    delete paths
}
/^[a-f0-9]+, [0-9]+ B/ { emit(); next }
/^    \//              { paths[++n] = substr($0, 5); next }
END                    { emit() }
' "$WORK/fclones.txt" \
    | LC_ALL=C sort > "$WORK/fclones-groups.txt"

awk '
function emit(   i, n_sorted, line) {
    if (n < 2) { n = 0; return }
    n_sorted = asort(paths)
    line = paths[1]
    for (i = 2; i <= n_sorted; i++) line = line "\t" paths[i]
    print line
    n = 0
    delete paths
}
/^$/ { emit(); next }
     { paths[++n] = $0 }
END  { emit() }
' "$WORK/jdupes.txt" \
    | LC_ALL=C sort > "$WORK/jdupes-groups.txt"

# ---- counts ----
n_bcd=$(wc -l < "$WORK/bcd-groups.txt")
n_fclones=$(wc -l < "$WORK/fclones-groups.txt")
n_jdupes=$(wc -l < "$WORK/jdupes-groups.txt")
echo "" | log
echo "duplicate groups reported:" | log
printf "  bc-duplicate : %d\n" "$n_bcd" | log
printf "  fclones      : %d\n" "$n_fclones" | log
printf "  jdupes       : %d\n" "$n_jdupes" | log

# ---- pairwise diffs ----
echo "" | log
echo "pairwise group-set comparison (lines unique per side):" | log
for pair in "bcd jdupes" "bcd fclones" "jdupes fclones"; do
    set -- $pair
    only_a=$(comm -23 "$WORK/$1-groups.txt" "$WORK/$2-groups.txt" | wc -l)
    only_b=$(comm -13 "$WORK/$1-groups.txt" "$WORK/$2-groups.txt" | wc -l)
    common=$(comm -12 "$WORK/$1-groups.txt" "$WORK/$2-groups.txt" | wc -l)
    printf "  %s vs %s: common=%d  only-%s=%d  only-%s=%d\n" \
        "$1" "$2" "$common" "$1" "$only_a" "$2" "$only_b" | log
done

# ---- sample diffs ----
echo "" | log
echo "first 3 groups only in bcd vs jdupes:" | log
comm -23 "$WORK/bcd-groups.txt" "$WORK/jdupes-groups.txt" | head -3 \
    | awk -F'\t' '{ print "  group of " NF " files: " $1 " ..." }' | log
echo "" | log
echo "first 3 groups only in jdupes vs bcd:" | log
comm -13 "$WORK/bcd-groups.txt" "$WORK/jdupes-groups.txt" | head -3 \
    | awk -F'\t' '{ print "  group of " NF " files: " $1 " ..." }' | log
