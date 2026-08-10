# EXP-20260810-003-gps-jump

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260810-003-gps-jump |
| Date | 2026-08-10 |
| Goal | Check that the jump profile reaches the twin as specified: a single step of the declared size along the declared bearing, held for the rest of the flight, with the reported velocity untouched |
| Commit | 68eb409 |
| Seed(s) | 1 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | GPS jump |
| Attack onset | 20 s declared, ±2 s jitter drawn from the seed, 20.05 s drawn |
| Attack parameters | `scenarios/ATK-003-gps-jump.yaml`: 50 m at bearing 60° |
| Repetitions | 1 |
| Metrics | separation between raw and twin position at onset and at the last fix, velocity difference between raw and twin, agreement between the twin's ground speed and its velocity components |
| Data | `data/20260810_square_ATK-003-gps-jump_s1`, results.csv |

## Notes

The jump is the easy case by design, and it is the only one of the three that is deliberately
incoherent. A constant offset has no derivative, so the velocity fields pass through untouched
and the fix reports a position step that its own velocity cannot account for. That is what a
kinematic gate is for, and it is the counterpart to the drift, which is built to avoid exactly
that inconsistency.

Onset is counted from the relay's first GPS message. The relay comes up before `ros2 bag
record` subscribes, so the step lands earlier in bag time than the drawn onset.
