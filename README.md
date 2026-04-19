# bc-duplicate

Fast, multi-threaded duplicate-file finder for Linux, part of the BitCrafts C
toolchain.

Walks directory trees and reports groups of identical files using a
size -> partial-hash -> full-hash pipeline. Hashes with xxh3 by default
(or xxh128 / SHA-256 on demand), with batched I/O via io_uring when
available.

## Subcommands

| Command | Purpose |
|---|---|
| `bc-duplicate scan <path>...` | Print groups of duplicate files |
| `bc-duplicate summary <path>...` | Print statistics only (files/groups/wasted bytes) |

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
| `--output auto\|-\|PATH` | `auto` | Output destination (file -> JSON, stdout -> jdupes-style) |

Global: `--threads auto|0|N` (auto-detected, single-threaded, or N workers).

## Default behaviour

- Hardlinks: deduplicated by `(dev, ino)` before hashing — silently collapsed.
- Hidden files: skipped.
- Symlinks: not followed (`lstat()` only).
- Pseudo-filesystems (`/proc`, `/sys`, `/dev`, ...): always skipped via statfs.
- Special files (devices, sockets, FIFOs): skipped.

## Build

```
bc-build bc-duplicate
bc-build bc-duplicate -Dtests=true -Dbenchmarks=true
bc-test bc-duplicate
bc-sanitize bc-duplicate
bc-check bc-duplicate
bc-bench bc-duplicate
bc-install bc-duplicate
```
