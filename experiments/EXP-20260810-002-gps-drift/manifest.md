# EXP-20260810-002-gps-drift

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260810-002-gps-drift |
| Date | 2026-08-10 |
| Goal | Check that the drift profile reaches the twin as specified: a smooth takeover, then a steady 1 m/s pull along the declared bearing, with the reported velocity matching the position it implies |
| Commit | 68eb409 |
| Seed(s) | 1 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | GPS drift |
| Attack onset | 20 s declared, ±2 s jitter drawn from the seed, 20.05 s drawn |
| Attack parameters | `scenarios/ATK-002-gps-drift.yaml`: 1.0 m/s at bearing 60°, 5 s ramp |
| Repetitions | 1 |
| Metrics | separation between raw and twin position against the offset the scenario predicts, velocity bias after the ramp, agreement between the twin's ground speed and its velocity components |
| Data | `data/20260810_square_ATK-002-gps-drift_s1`, results.csv |

## Notes

The offset is defined as a ramp to the declared rate rather than a step onto it: it grows as
½·(rate/ramp)·t² for the first 5 s and as rate·(t − ramp/2) after that. The reported velocity
is the derivative of that curve, so the twin's position and velocity agree with each other
throughout. A step would put a 1 m/s discontinuity in the velocity at onset, which is a
different attack and a much easier one to catch.

Onset is counted from the relay's first GPS message, which is the value the relay logs at
startup. The relay comes up before `ros2 bag record` subscribes, so the divergence lands
earlier in bag time than the drawn onset. EXP-20260810-001 saw the same offset and the size of
it is not repeatable, so bag time is not attack time.
