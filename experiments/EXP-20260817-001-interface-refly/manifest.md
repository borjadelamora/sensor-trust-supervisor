# EXP-20260817-001-interface-refly

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260817-001-interface-refly |
| Date | 2026-08-17 |
| Goal | Whether moving both monitors onto one interface left their residuals and levels unchanged, and what level the GNSS monitor reports now that it has a debounce |
| Commit | 77823bc |
| Seed(s) | 1 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | none (benign), GPS freeze (ATK-001), flow corruption (ATK-004) |
| Attack onset | 20 s ± 2 s jitter for both, from the relay's first px4 stamp |
| Attack parameters | ATK-001 freeze, no duration. ATK-004 flow scale 0.5, linear rise 10 s, ease 1 s |
| Repetitions | one run per scenario |
| Metrics | Peak, median and 95th percentile of each residual, and the SUSPECT episodes each monitor raised. Flow compares against EXP-20260816-001 and GNSS against EXP-20260815-001, both on this seed. GNSS's threshold is 2.0 m/s held for 2.0 s. |
| Data | `data/20260817_square_{none,ATK-001-gps-freeze,ATK-004-flow-corruption}_s1`, results.csv |

## Notes

Flow's residual maths and debounce carried over intact, so its numbers are the ones that have to
match. The GNSS level is new this session, at a threshold above the 1.40 m/s benign ceiling
EXP-20260815-001 measured.
