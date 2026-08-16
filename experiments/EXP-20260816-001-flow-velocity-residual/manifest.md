# EXP-20260816-001-flow-velocity-residual

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260816-001-flow-velocity-residual |
| Date | 2026-08-16 |
| Goal | Whether the flow velocity residual and its debounce separate a corrupted flow reading from a benign flight |
| Commit | ab7c823 |
| Seed(s) | 1 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | none (benign), flow corruption (ATK-004), GPS drift with flow corruption (ATK-007) |
| Attack onset | 20 s ± 2 s jitter for GPS and ATK-004's flow, 26 s ± 2 s for ATK-007's flow, from the relay's first px4 stamp |
| Attack parameters | ATK-004 flow scale 0.5, linear rise 10 s, ease 1 s. ATK-007 GPS 47 m at 060° over 49.5 s, flow scale 0.7 over 10 s |
| Repetitions | one run per scenario |
| Metrics | Horizontal residual per run, in m/s, against a 1.0 m/s threshold held for 2.0 s. Latency runs from the delivered onset to the first SUSPECT. |
| Data | `data/20260816_square_{none,ATK-004-flow-corruption,ATK-007-gps-drift-flow-corruption}_s1`, results.csv |

## Notes

Both threshold and sustain period are provisional. The 1.0 m/s and 2.0 s figures come from the
research rule, which was written for flow against true speed, and this residual is against a
dead-reckoned reference that re-anchors every 10 s. Delivered onsets are `analyse_run.py`'s, so
every time here is in the relay's clock.
