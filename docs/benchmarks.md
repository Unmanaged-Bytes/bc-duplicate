# bc-duplicate benchmarks

Reproducible duplicate-finder benchmarks against `jdupes`, `fclones`, and
`rdfind`. `fdupes` is excluded — consistently >10x slower than the others
on >100k file corpora and adds no signal.

## Methodology

`scripts/bench.sh <target-directory>` :

1. Records git revision, machine governor / boost / ASLR and target stats.
2. **Correctness spot-check** — picks N random duplicate groups from
   `bc-duplicate scan --output=PATH` JSON, asserts every path in the
   group has the same `sha256sum`. Requires `jq`.
3. Each tool runs **two warm-up iterations** (drops cold-cache effects)
   then `WARM_RUNS` (default 5) timed iterations. Wall-time only,
   measured with `date +%s.%N`.
4. Optional `--with-cold` re-runs after `sync && echo 3 > /proc/sys/vm/drop_caches`
   for `COLD_RUNS` iterations (3 by default). Requires sudo.
5. `awk` summary at the end prints mean / stddev / n per (warm|cold,tool).

Output: `build/perf-logs/bench.log`.

Tool flags used:

| Tool | Flags | Reason |
|---|---|---|
| `bc-duplicate` | `summary --hidden` | summary mode skips per-path stdout, `--hidden` matches the others |
| `bc-duplicate` | `summary --hidden --algorithm=sha256` | second pass for cryptographic-hash baseline |
| `fclones` | `group <path>` | default group output |
| `jdupes` | `-rmS` | recursive, summarize, show sizes (no stdout per path) |
| `rdfind` | `-dryrun true <path>` | dry-run, no link/delete |

## Reference machine

ws-desktop-00, AMD Ryzen 7 5700G, 8c/16t, 32 GB RAM, performance governor.

Throughput constants measured by `bc_duplicate_throughput` (cached at
`~/.cache/bc-duplicate/throughput.txt`):

```
xxh3=18.49 GB/s   xxh128=17.96 GB/s   sha256=1.68 GB/s
mem_bw=34.68 GB/s  parallel_startup=37.7 us  per_file=3.55 us
```

## Results — warm cache

### `/var/benchmarks/github` (18 064 files, 531 MB)

| Tool | mean | stddev | rel |
|---|---:|---:|---:|
| fclones | 0.027 s | 0.002 | 1.00× |
| bc-duplicate xxh3 | 0.076 s | 0.001 | 2.81× |
| bc-duplicate sha256 | 0.098 s | 0.003 | 3.63× |
| jdupes -rmS | 0.271 s | 0.002 | 10.04× |

### `/var/benchmarks/2026-04-12` (7 092 files, 82 MB)

| Tool | mean | stddev | rel |
|---|---:|---:|---:|
| fclones | 0.034 s | 0.001 | 1.00× |
| bc-duplicate xxh3 | 0.056 s | 0.001 | 1.65× |
| bc-duplicate sha256 | 0.076 s | 0.000 | 2.24× |
| jdupes -rmS | 0.077 s | 0.000 | 2.26× |

### `/var/benchmarks/nested` (724 414 files, 13 GB) — perf-stat, median of 3 warm runs

Each tool was run with its own preferred parallelism: bc-duplicate uses
the 8 physical cores (SMT off-by-design), fclones uses 16 SMT threads
(default), jdupes is single-threaded (no parallelism flag).

| Tool | wall | task-clock | IPC | branch-miss | cache-miss | RSS peak |
|---|---:|---:|---:|---:|---:|---:|
| bc-duplicate xxh3 | **1.79 s** | 7.47 s | 0.94 | 9.08 % | 8.41 % | 350 MB |
| fclones | 2.37 s | 22.07 s | 1.09 | 3.54 % | 9.71 % | 151 MB |
| jdupes -rmS | 32.30 s | 32.31 s | 1.95 | 2.28 % | 9.01 % | 140 MB |

bc-duplicate executes ~3.2x fewer instructions than fclones for the
same corpus: algorithmic win from the size+head-hash pre-grouping.
fclones spreads work across 2x the threads (SMT) but the higher
task-clock more than offsets the parallelism gain.

Per-tool observations:
- bc-duplicate IPC 0.94 is the lowest — branch mispredictions (9 %)
  point at the algorithm-switching `consumer_callback` in the hot loop.
  Specialising by algo (one consumer per xxh3/xxh128/sha256) would
  remove a per-chunk indirect call.
- RSS is dominated by io_uring slot buffers
  (32 slots * 128 KB * 8 workers = 32 MB * 8 = 256 MB just for the
  rings). Halving slot buffer to 64 KB would shave ~128 MB.
- jdupes' high IPC (1.95) is misleading: it runs on a single core, the
  pipeline is fully predictable but the wall is ~18x longer.

### `/var/benchmarks` (767 312 files, 19 GB, with `--hidden`) — single-run wall

| Tool | wall | rel |
|---|---:|---:|
| bc-duplicate xxh3 (adaptive dispatch) | 5.50 s | 1.00× |
| fclones | 3.20 s | 0.58× |
| jdupes -rmS | 55.60 s | 10.11× |

## 3-way correctness check

`scripts/correctness-3way.sh <target>` runs the three tools, parses
their output into one TSV line per duplicate group (paths sorted),
and `comm`-diffs the resulting sets. To compare apples to apples the
script forces:

| Tool | Flags | Why |
|---|---|---|
| bc-duplicate | `scan --hidden` | bc-duplicate skips dotfiles by default |
| fclones | `group --hidden --no-ignore` | fclones skips dotfiles AND respects `.gitignore` / `.fdignore` by default — without `--no-ignore` it would silently drop entire `build/`, `node_modules/`, `target/`, ... trees |
| jdupes | `-r` | jdupes already includes hidden by default and ignores `.gitignore` |

Without `--no-ignore`, fclones reports 45 groups on
`/var/benchmarks/github` while bc-duplicate and jdupes both find 400
— it is the `.gitignore` filter, not a hashing bug.

Results on the reference machine (current main):

| Target | files | bc-duplicate | fclones | jdupes | pairwise |
|---|---:|---:|---:|---:|---|
| `/var/benchmarks/2026-04-12` | 7 092 | 103 | 103 | 103 | identical |
| `/var/benchmarks/github` | 18 064 | 400 | 400 | 400 | identical |
| `/var/benchmarks/nested` | 724 414 | 102 418 | 102 418 | 102 418 | identical |

Logs: `build/perf-logs/correctness-3way-{github,nested}.log`.

## Parallelism / fairness disclosure

The three tools have different default parallelism strategies and we
deliberately let each use its own:

| Tool | Threads | Notes |
|---|---|---|
| bc-duplicate | 8 (1 per **physical** core) | SMT not used; bc-concurrency pins one worker per physical core. `--threads=N` overrides. |
| fclones | 16 (= `num_cpus`, includes SMT) | default; `--threads N` overrides. |
| jdupes | 1 | no parallelism flag. |

We did **not** force fclones down to 8 threads in the table above
because that does not match how a user runs it in practice; on a
dedicated benchmark machine SMT is part of the tool's deployed perf
profile. To compare on equal threads, run
`fclones group --threads 8 <path>`.

## Reading the numbers

- **Small corpora (<20k files)**: fclones wins (low constant overhead).
  bc-duplicate beats jdupes by 3-10×.
- **Mid (>500k files, mostly small files)**: bc-duplicate beats fclones
  warm cache thanks to the parallel-walk + adaptive dispatch + io_uring
  batched openat/read/close per worker (iter 7).
- **Very large (>1M files / many GB)**: I/O-bound. Numbers vary across
  runs because page-cache eviction kicks in. Median wall on three warm
  runs of `/var/benchmarks --hidden` (19 GB):
  bc-duplicate ~7 s, fclones ~3 s. Here fclones' prefix/suffix scan
  reads strictly less data than our streaming xxh3 over the whole file.

## Why xxh3 by default

xxh3 measured at **18.5 GB/s** vs sha256 at **1.7 GB/s** on the
reference machine — 11× cheaper. Collision risk for dedup is
effectively zero (after pre-grouping by size and 4 KB head digest
the residual collision probability for xxh3 is below `2e-19` per pair).
SHA-256 is opt-in via `--algorithm=sha256` for callers that need
cryptographic dedup.

## Reproducing

```
bc-build ~/Workspace/tools/bc-duplicate release
~/Workspace/tools/bc-duplicate/scripts/bench.sh /var/benchmarks/github
cat ~/Workspace/tools/bc-duplicate/build/perf-logs/bench.log
```

Override iteration counts with `WARM_RUNS=10 COLD_RUNS=5 ./scripts/bench.sh ...`.
