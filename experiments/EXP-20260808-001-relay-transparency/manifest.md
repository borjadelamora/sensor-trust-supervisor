# EXP-20260808-001-relay-transparency

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260808-001-relay-transparency |
| Date | 2026-08-08 |
| Goal | Confirm the injector relay is a no-op: a flight through it matches the raw topics |
| Commit | 5e4e9ad |
| Seed(s) | 1 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | none (benign) |
| Attack onset |—|
| Attack parameters |—|
| Repetitions | 1 |
| Metrics | per-topic message counts, source messages matched to a twin by timestamp, field mismatches |
| Data | `data/20260808_square_s1`, results.csv |

## Notes

The relay mirrors four channels: `vehicle_gps_position`, `sensor_combined`,
`sensor_optical_flow` and `vehicle_optical_flow_vel`. Both the raw and the relayed topics were
recorded in the same bag, so the comparison is within a single flight rather than between two.

Comparing serialised bytes rather than fields reports every `SensorGps` and
`SensorOpticalFlow` message as different. The padding in those message types is filled
differently by the PX4 client and by rclpy, so the comparison has to be on deserialised
fields. `SensorGps.heading` and `heading_offset` are NaN throughout, which also has to be
handled or every message reads as a mismatch.
