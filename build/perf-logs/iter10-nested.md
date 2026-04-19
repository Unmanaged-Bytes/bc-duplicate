| Command | Mean [s] | Min [s] | Max [s] | Relative |
|:---|---:|---:|---:|---:|
| `build/release/src/bc-duplicate summary /var/benchmarks/nested` | 1.828 ± 0.012 | 1.813 | 1.841 | 1.00 |
| `fclones group --hidden --no-ignore /var/benchmarks/nested 2>/dev/null` | 2.578 ± 0.036 | 2.542 | 2.638 | 1.41 ± 0.02 |
| `fclones group --hidden --no-ignore --threads 8 /var/benchmarks/nested 2>/dev/null` | 2.954 ± 0.034 | 2.920 | 3.004 | 1.62 ± 0.02 |
| `jdupes -rmS /var/benchmarks/nested 2>/dev/null` | 32.251 ± 0.101 | 32.119 | 32.395 | 17.65 ± 0.13 |
