| Command | Mean [s] | Min [s] | Max [s] | Relative |
|:---|---:|---:|---:|---:|
| `build/release/src/bc-duplicate summary --hidden /var/benchmarks` | 3.700 ± 0.032 | 3.654 | 3.730 | 1.00 |
| `fclones group --hidden --no-ignore /var/benchmarks 2>/dev/null` | 3.927 ± 0.087 | 3.865 | 4.075 | 1.06 ± 0.03 |
| `fclones group --hidden --no-ignore --threads 8 /var/benchmarks 2>/dev/null` | 4.589 ± 0.016 | 4.575 | 4.613 | 1.24 ± 0.01 |
| `jdupes -r /var/benchmarks 2>/dev/null >/dev/null` | 41.512 ± 0.078 | 41.407 | 41.592 | 11.22 ± 0.10 |
