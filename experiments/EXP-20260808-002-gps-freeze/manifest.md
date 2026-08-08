# EXP-20260808-002-gps-freeze

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260808-002-gps-freeze |
| Date | 2026-08-08 |
| Goal | First corruption through the relay: hold the last GPS fix and measure how far the twin drifts from the raw topic |
| Commit | adca71f |
| Seed(s) | 2 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | GPS freeze |
| Attack onset | 20 s after the relay's first GPS message |
| Attack parameters | hardcoded, no scenario file yet |
| Repetitions | 1 |
| Metrics | onset time in the bag, messages identical before onset, peak separation between raw and twin position |
| Data | `data/20260808_square_s2`, results.csv |

## Notes

Onset is measured from the first GPS message the relay sees. The relay comes up before
`ros2 bag record` does, so the freeze lands 17.7 s into the recording.

The vehicle flew the square as usual. The offboard node navigates on
`/fmu/out/vehicle_local_position_v1` and PX4's EKF2 reads its own sensors, so neither of them
sees the relayed topics and the flight path is unaffected. That is what injecting at topic
level buys: the corruption reaches only the layer being built, and PX4 stays untouched. The
cost is that this run says nothing about how a vehicle would behave under a real spoof.

The bag name carries the mission and the seed but not the attack, so this run looks benign from
the file tree alone. Scenario ids will carry that.
