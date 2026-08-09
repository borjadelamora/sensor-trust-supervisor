# EXP-20260810-001-scenario-replay

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260810-001-scenario-replay |
| Date | 2026-08-10 |
| Goal | Check that a scenario file drives the freeze end to end, and that a seed replays the same onset while a different seed moves it |
| Commit | 7af0a67 |
| Seed(s) | 1, 2 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | GPS freeze |
| Attack onset | 20 s declared, ±2 s jitter drawn from the seed |
| Attack parameters | `scenarios/ATK-001-gps-freeze.yaml` |
| Repetitions | 2, one per seed |
| Metrics | drawn onset in the relay log, onset observed in the twin, agreement between the two, separation between raw and twin position |
| Data | `data/20260810_square_ATK-001-gps-freeze_s1`, `data/20260810_square_ATK-001-gps-freeze_s2`, results.csv |

## Notes

The session this belongs to ran on the 9th; both flights went off just after midnight, so the
id and the bags carry the 10th.

Replaying the same seed is covered by the unit tests, which need no simulator. The two flights
here use different seeds, so the drawn onsets should differ while everything else about the
scenario holds.
