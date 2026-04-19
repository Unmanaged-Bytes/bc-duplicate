| Command | Mean [s] | Min [s] | Max [s] | Relative |
|:---|---:|---:|---:|---:|
| `build/release/src/bc-duplicate summary /var/benchmarks/nested` | 2.730 ± 0.027 | 2.700 | 2.771 | 1.00 |
| `fclones group --hidden --no-ignore /var/benchmarks/nested 2>/dev/null` | 2.760 ± 0.020 | 2.733 | 2.781 | 1.01 ± 0.01 |
| `fclones group --hidden --no-ignore --threads 8 /var/benchmarks/nested 2>/dev/null` | 3.510 ± 0.333 | 3.291 | 4.071 | 1.29 ± 0.12 |
| `jdupes -rmS /var/benchmarks/nested 2>/dev/null` | 32.581 ± 0.353 | 32.301 | 33.173 | 11.93 ± 0.17 |
