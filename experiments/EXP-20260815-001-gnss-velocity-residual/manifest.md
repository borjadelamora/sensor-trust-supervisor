# EXP-20260815-001-gnss-velocity-residual

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260815-001-gnss-velocity-residual |
| Date | 2026-08-15 |
| Goal | Whether the GNSS velocity residual separates a spoofed fix from a benign flight |
| Commit | 5ec7f78 |
| Seed(s) | 1 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | none (benign), GPS freeze (ATK-001), GPS drift (ATK-002) |
| Attack onset | 20 s ± 2 s jitter, from the relay's first px4 stamp |
| Attack parameters | ATK-001 freeze, no duration. ATK-002 47 m at 060°, linear rise 49.5 s, ease 2.5 s |
| Repetitions | one run per scenario |
| Metrics | Peak horizontal residual per run, in m/s. Detection is a crossing of 1.40 m/s, the ceiling from the 22:07 debug benign flight. Latency runs from the delivered onset to the first crossing. |
| Data | `data/20260815_square_{none,ATK-001-gps-freeze,ATK-002-gps-drift}_s1`, results.csv |

## Notes

The 1.40 m/s ceiling comes from the 22:07 debug flight, where it sits in the last ten seconds
of the descent. The same seed gave 0.556 m/s two hours earlier. Horizontal component only.
