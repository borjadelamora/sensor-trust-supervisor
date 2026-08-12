# EXP-20260812-001-gps-freeze

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260812-001-gps-freeze |
| Date | 2026-08-12 |
| Goal | Check the frozen fix holds a single position and reports itself as stationary, now that the course field is zeroed with the rest, and re-establish the freeze result on the shared clock |
| Commit | cc3eb2a |
| Seed(s) | 1 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | GPS freeze |
| Attack onset | 20 s declared, ±2 s jitter drawn from the seed, 20.047 s drawn, 20.651 s delivered |
| Attack parameters | `scenarios/ATK-001-gps-freeze.yaml`: none, the fix holds from onset to the end of the flight |
| Repetitions | 1 |
| Metrics | how many distinct positions the twin reports after onset, the velocity and course fields on held fixes, separation between raw and twin, delivered onset against drawn |
| Data | `data/20260812_square_ATK-001-gps-freeze_s1`, results.csv |

## Notes

This supersedes EXP-20260808-002 as a description of what the freeze does. That run held the
position and zeroed the velocity but passed the true course through, so the twin claimed to be
stationary while still publishing the heading the vehicle was flying. The course is now zeroed
with the velocity. Real receivers vary here, NMEA voids the course field below a speed threshold
while others hold the last value, so this is a coherence choice rather than a physical necessity
and [ADR-0004](../../docs/decisions/0004-injector-at-topic-level.md) records it as one.

Onset is counted from the first PX4 stamp the relay sees, which the relay writes into
`relay.log` beside the bag. The delivered onset is 0.60 s after the drawn one because PX4's
stamps arrive in bursts on this machine: nothing is published across the burst, so the attack
begins on the first fix after it. The largest gap in this run is 4.17 s.

Three messages arrived carrying stamps from before the run started. They are relayed as they
came but they do not move the clock, so they cannot disable the attack.
