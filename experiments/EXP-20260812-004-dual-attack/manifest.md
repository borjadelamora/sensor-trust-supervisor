# EXP-20260812-004-dual-attack

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260812-004-dual-attack |
| Date | 2026-08-12 |
| Goal | Two channels attacked in one flight. Check the drift and the blackout each land where the scenario declares against a single time reference, that the third channel stays clean throughout, and see what the spacing between two onsets actually comes out as |
| Commit | cc3eb2a |
| Seed(s) | 1 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | GPS drift and camera blackout |
| Attack onset | GPS 20 s declared, 20.047 s drawn, 20.059 s delivered. Camera 26 s declared, 24.577 s drawn, 28.307 s delivered. Both ±2 s jitter from the seed |
| Attack parameters | `scenarios/ATK-006-gps-drift-camera-blackout.yaml`: GPS 1.0 m/s at bearing 60° with a 5 s ramp, camera intensity 0 for 15 s |
| Repetitions | 1 |
| Metrics | separation against the offset the scenario predicts, velocity coherence on the drifted fix, frames inside the declared camera window against frames flooded, whether the flow channel is touched, spacing between the two delivered onsets |
| Data | `data/20260812_square_ATK-006-gps-drift-camera-blackout_s1`, results.csv |

## Notes

This is the scenario that forced the timing model. Both channels take their elapsed time from one
reference, the first PX4 stamp the relay sees, written into `relay.log` beside the bag. Before
that each channel counted from the first message on its own stream, and since the streams start
at different moments and the camera was being timed by a different clock entirely, the spacing
between the two onsets was not the spacing the file declared and was not reproducible either.
[ADR-0004](../../docs/decisions/0004-injector-at-topic-level.md) has the reasoning.

The declared spacing is 6 s. The jitter draws pull it to 4.53 s, which is the number to hold the
run against, not the 6.

Onset delivery is bounded by how often a PX4 stamp arrives. This machine runs the simulation
below real time and then catches up in bursts, dropping most of what PX4 emits while it does, so
the clock can cross a declared onset with no message in between to corrupt.
