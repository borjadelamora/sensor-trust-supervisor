# EXP-20260812-003-camera-blackout

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260812-003-camera-blackout |
| Date | 2026-08-12 |
| Goal | Check every frame inside the declared window is flooded and no frame outside it is touched, and that a flooded frame is still a readable image |
| Commit | cc3eb2a |
| Seed(s) | 1 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | Camera blackout |
| Attack onset | 20 s declared, ±2 s jitter drawn from the seed, 18.577 s drawn, 18.589 s delivered |
| Attack parameters | `scenarios/ATK-005-camera-blackout.yaml`: intensity 0, duration 15 s |
| Repetitions | 1 |
| Metrics | frames inside the declared window against frames flooded, frames outside against frames untouched, frames left partly corrupted, frame size and encoding after flooding, delivered onset against drawn |
| Data | `data/20260812_square_ATK-005-camera-blackout_s1`, results.csv |

## Notes

The camera is the one relayed stream carrying no PX4 stamp of its own. Its frames come from
Gazebo through a second bridge, and their `header.stamp` is Gazebo's clock, which does not run at
the same rate as PX4's stamps. Timing them by their own clock and the GPS by its own is what
sends a two-channel scenario off by seconds, so frames are timed by the most recent PX4 stamp the
relay has seen instead. `sensor_combined` arrives at 100 Hz, so that stamp is never more than
10 ms old.

The measurement to be careful with is the window. The span between the first and last flooded
frame is not the duration: it depends on how many frames the renderer produced during the window,
which varies. The number that means something is membership, every frame whose elapsed time falls
inside `[onset, onset + duration)` against every frame that was actually flooded.
