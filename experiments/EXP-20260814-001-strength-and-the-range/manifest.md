# EXP-20260814-001-strength-and-the-range

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260814-001-strength-and-the-range |
| Date | 2026-08-14 |
| Goal | Re-fly the whole battery after intensity became a strength and the range gained a twin. Check each attack reaches the magnitude its scenario declares, that a lens attack takes the flow with it in proportion and that the reported range moves with the velocity it scales into |
| Commit | 5744707 |
| Seed(s) | 1 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | Benign, then ATK-001 to ATK-011 |
| Attack onset | 20 s declared on most channels with ±2 s jitter drawn from the seed. Delivered onsets are in each bag's `relay.log` and in results.csv, both now on the relay's own clock |
| Attack parameters | `scenarios/ATK-001` to `ATK-011`. Step, linear, sigmoid and oscillating profiles over a 14 m to 50 m offset on bearing 60° for the spoofs. Flow scales of 0.5 and 0.7. Camera intensity 1.0, toward black or toward saturation |
| Repetitions | 1 per scenario, 12 runs |
| Metrics | peak separation between the twin fix and its source against the offset the scenario declares, ratio of twin to source for flow velocity, raw pixel flow and reported range, each taken where it sits furthest from unchanged, mean frame level at the frame furthest from its source, corrupted against matched messages per channel and whether an unattacked channel is touched |
| Data | `data/20260814_square_*`, results.csv |

## Notes

`tools/analyse_run.py` writes results.csv from the bags, a row per channel per run. Two metrics
changed from the previous battery for the same reason. A window holds a ramp and a plateau, and the
simulator samples it unevenly, so a median or an average over the window describes how the window
was sampled. Every magnitude is now taken where it sits furthest from the untouched value. On
ATK-011 the median read 0.4608 for a lens the frames show reaching full saturation, which is what
prompted the change.

A `clock` column says which clock a row's times are on. Gazebo stamps the frames and its clock is
neither the epoch nor the rate px4 counts in, 12.26 s of frame time against 14.98 s of px4 time in
one measurement, so a camera time compares only against another camera time. Every other channel
counts from the px4 stamp the relay recorded as its origin and lands on the delivered onset.

Stamp gaps run from 4.98 s to 7.97 s across the twelve runs, larger than the 4.17 s to 6.13 s of
12 Aug. Two results below are shaped by one. Each is noted where it appears.
