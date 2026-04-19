# Changelog

## 1.0.0 (unreleased)

Initial release.

- Subcommands: `scan` (groups of duplicate paths) and `summary` (statistics).
- Algorithms: xxh3 (default), xxh128, SHA-256.
- Discovery: parallel walk via per-worker bounded MPMC queue, per-worker
  vectors merged after a barrier. Skips dotfiles, symlinks,
  pseudo-filesystems and non-regular files by default.
- Hardlink dedup by `(dev, ino)` (opt-out with `--match-hardlinks`).
- Fast pass: `pread()` 4 KB prefix and (for files ≥ 8 KB) 4 KB suffix,
  XOR-combined into a single 64-bit fast hash to discard non-matches
  before the full pass.
- Adaptive single vs multi-thread dispatch for the full pass, driven
  by a one-shot hardware throughput measurement cached at
  `$XDG_CACHE_HOME/bc-duplicate/throughput.txt`.
- Full pass: streaming hash with per-worker io_uring rings (32 slots
  × 256 KB direct-fd buffers, chained `openat_direct` + `read` +
  `close_direct`). Sync streaming fallback when liburing is absent or
  the file is larger than the slot.
- Output: jdupes-style on a TTY, JSON on a pipe or with `--output PATH`.
- Cmocka tests covering CLI binding, filter, grouping, output,
  throughput measurement and dispatch decision; end-to-end tests
  covering discovery, full pipeline, JSON output and `--output=-`.
- Sanitizers (asan, ubsan, tsan) and cppcheck stay green in CI.
- 3-way correctness check vs `fclones` and `jdupes` reports identical
  duplicate-group sets on the reference benchmark corpora.
