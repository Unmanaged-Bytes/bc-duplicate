# bc-duplicate

Fast, multi-threaded duplicate-file finder for Linux, part of the BitCrafts C
toolchain.

Walks directory trees and reports groups of identical files using a
size → fast-hash (4 KB prefix XOR 4 KB suffix) → full-hash pipeline.
Hashes with xxh3 by default (or xxh128 / SHA-256 on demand). The
full-hash pass uses one io_uring ring per worker for batched
`openat_direct` + `read` + `close_direct` (liburing ≥ 2.5 is a build
dependency).

## Subcommands

| Command | Purpose |
|---|---|
| `bc-duplicate scan <path>...` | Print groups of duplicate files |
| `bc-duplicate summary <path>...` | Print statistics only (files / groups / wasted bytes) |

## Options (scan)

| Flag | Default | Description |
|---|---|---|
| `--algorithm xxh3\|xxh128\|sha256` | `xxh3` | Full-file hash algorithm |
| `--minimum-size BYTES` | `1` | Skip files smaller than BYTES |
| `--include GLOB` | — | Only include files whose basename matches |
| `--exclude GLOB` | — | Skip files or directories whose basename matches |
| `--hidden` | off | Include hidden files (basename starting with `.`) |
| `--follow-symlinks` | off | Follow symbolic links |
| `--match-hardlinks` | off | Report files sharing an inode as duplicates |
| `--one-file-system` | off | Do not cross filesystem boundaries |
| `--output auto\|-\|PATH` | `auto` | Output destination (file → JSON, stdout → jdupes-style) |

Global: `--threads auto|0|N` (auto-detected, single-threaded, or N workers).

## Default behaviour

- Hardlinks: deduplicated by `(dev, ino)` before hashing — silently collapsed.
- Hidden files: skipped.
- Symlinks: not followed (`lstat()` only).
- Pseudo-filesystems (`/proc`, `/sys`, `/dev`, ...): always skipped via `statfs()`.
- Special files (devices, sockets, FIFOs): skipped.

## Build

```
bc-build bc-duplicate          # release
bc-test bc-duplicate           # cmocka suite
bc-sanitize bc-duplicate asan  # also ubsan / tsan / memcheck
bc-check bc-duplicate          # cppcheck
bc-install bc-duplicate
```

## Performance

Reference machine: AMD Ryzen 7 5700G (Zen 3, 8 physical cores / 16
threads), 32 GB DDR4-3200, NVMe SSD. Release build with
`-march=x86-64-v3 -funroll-loops -fomit-frame-pointer
-ffunction-sections -fdata-sections -Wl,--gc-sections`. CPU governor
set to performance, ASLR disabled, page cache warm.

Wall time (`hyperfine --warmup 2 --runs 5`) compared to
`fclones 0.35.0` and `jdupes 1.28.0`. `fdupes` is excluded — it is
consistently >10× slower on >100 k file corpora and adds no signal.

bc-duplicate is invoked with its defaults (`--threads auto` = one
worker per physical core, no SMT). `fclones` and `jdupes` are invoked
with their own defaults to match how a user runs them; where relevant
we also report `fclones --threads 8` for an apples-to-apples
physical-thread-count comparison.

To compare the same filesystem scope (the three tools have different
defaults around dotfiles and `.gitignore`), bc-duplicate is run with
`--hidden` and fclones with `--hidden --no-ignore` (see "Correctness"
below for why).

| Corpus | files | size | bc-duplicate | fclones (default 16 SMT) | fclones --threads 8 | jdupes |
|---|---:|---:|---:|---:|---:|---:|
| Small project tree | 7 092 | 82 MB | 51.2 ms | **34.5 ms** | n/a | 75.8 ms |
| Multi-repo checkout | 18 064 | 531 MB | 77.0 ms | **66.5 ms** | n/a | 267.8 ms |
| Nested mirror | 724 414 | 13 GB | **1.83 s** | 2.58 s | 2.95 s | 32.25 s |
| Whole benchmark root (`--hidden`) | 767 312 | 19 GB | **3.70 s** | 3.93 s | 4.59 s | 41.51 s |

- On small corpora (under ~50 k files) `fclones` wins on raw constant
  overhead; bc-duplicate is within 1.5×.
- On the two large corpora bc-duplicate is 1.06× and 1.41× faster than
  the default `fclones` invocation (16 SMT threads), and 1.24× / 1.62×
  faster than `fclones --threads 8` at matched physical-thread count.
- bc-duplicate is 11–18× faster than `jdupes` on the large corpora.

Throughput constants measured at first run by
`bc_duplicate_throughput` and cached under
`$XDG_CACHE_HOME/bc-duplicate/throughput.txt` (or
`~/.cache/bc-duplicate/throughput.txt`) so subsequent runs reuse them.
On the reference machine:

```
xxh3 = 18.5 GB/s   xxh128 = 18.0 GB/s   sha256 = 1.7 GB/s
mem_bw = 34.7 GB/s  parallel_startup = 38 µs  per_file = 3.6 µs
```

## Correctness

`scripts/correctness-3way.sh <target>` runs bc-duplicate, fclones and
jdupes, parses each output into one TSV line per duplicate group
(paths sorted), and `comm`-diffs the line sets. To compare apples to
apples the script forces:

| Tool | Flags | Why |
|---|---|---|
| `bc-duplicate` | `scan --hidden` | dotfiles are skipped by default |
| `fclones` | `group --hidden --no-ignore` | dotfiles AND `.gitignore` are honoured by default — without `--no-ignore`, fclones silently drops `build/`, `node_modules/`, `target/`, ... |
| `jdupes` | `-r` | already includes hidden by default and ignores `.gitignore` |

Without `--no-ignore`, fclones reports a strict subset of the groups
that bc-duplicate and jdupes find on a corpus that contains
`.gitignore` files (typically 10–20 % of the real groups) — it is the
gitignore filter at work, not a hashing discrepancy.

With matched scope, all three tools report identical groups:

| Target | files | bc-duplicate | fclones | jdupes | pairwise diff |
|---|---:|---:|---:|---:|---|
| Small project tree   |   7 092 |     103 |     103 |     103 | 0 |
| Multi-repo checkout  |  18 064 |     400 |     400 |     400 | 0 |
| Nested mirror        | 724 414 | 102 418 | 102 418 | 102 418 | 0 |

## Pipeline

1. **Discovery**: parallel walk via per-worker bounded MPMC queue,
   per-worker file-entry vectors merged after a barrier (no shared
   pool across workers). Skips dotfiles, symlinks, pseudo-filesystems
   and non-regular files by default.
2. **Group by size**: `qsort` on `(size desc, dev, ino)` then a
   linear scan; hardlinks are collapsed by `(dev, ino)` unless
   `--match-hardlinks` is set.
3. **Fast pass** (parallel via `bc_concurrency_for`): `pread()` 4 KB
   at offset 0 and (if `file_size ≥ 8 KB`) 4 KB at the file tail,
   xxh3 of each, XOR-combined into a single 64-bit fast hash.
4. **Group by fast hash**: per-size-group qsort + linear scan.
5. **Adaptive dispatch**: a one-shot hardware throughput measurement
   (xxh3 / xxh128 / sha256 GB/s, memory bandwidth, parallel startup
   overhead, per-file warm cost) is cached at
   `$XDG_CACHE_HOME/bc-duplicate/throughput.txt`. The full pass runs
   single-threaded if the predicted multi-threaded wall (mono / N +
   parallel-startup) is worse than mono.
6. **Full pass**: streaming xxh3 / xxh128 / sha256 over the whole
   file. Per-worker io_uring ring (32 slots × 256 KB direct-fd
   buffers) drives chained `openat_direct` + `read` + `close_direct`;
   files larger than the slot fall back to sync streaming reads.
7. **Group by full hash**: per-fast-hash-group qsort by digest +
   linear scan. Only groups of ≥2 entries are reported.
8. **Output**: jdupes-style on stdout if a TTY, JSON otherwise. The
   `summary` subcommand emits the statistics block instead.

## Why xxh3 by default

On a Zen 3 SHA-NI box xxh3 measures around 18 GB/s vs SHA-256 at
1.7 GB/s — about an order of magnitude cheaper for the same per-file
work. Collision risk for dedup is effectively zero after pre-grouping
by size and the 4 KB prefix + suffix fast hash. `--algorithm=sha256`
is opt-in for callers that want a cryptographic guarantee.

## Reproducing the benchmarks

```
bc-build bc-duplicate release
scripts/bench.sh <target-directory>
scripts/correctness-3way.sh <target-directory>
```

`scripts/bench.sh` writes `build/perf-logs/bench.log` with per-run
wall times and an awk-computed mean / stddev / n summary.
`scripts/correctness-3way.sh` writes
`build/perf-logs/correctness-3way.log`.
Override iteration counts with `WARM_RUNS=10 scripts/bench.sh ...`.
