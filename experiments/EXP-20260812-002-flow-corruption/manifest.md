# EXP-20260812-002-flow-corruption

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260812-002-flow-corruption |
| Date | 2026-08-12 |
| Goal | Check the flow attack scales the reported velocity and leaves everything the velocity is derived from alone, so the corrupted message stays consistent with the rates and the gyro beside it |
| Commit | cc3eb2a |
| Seed(s) | 1 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | Flow corruption |
| Attack onset | 20 s declared, ±2 s jitter drawn from the seed, 21.802 s drawn, 24.467 s delivered |
| Attack parameters | `scenarios/ATK-004-flow-corruption.yaml`: velocity scaled by 0.5 |
| Repetitions | 1 |
| Metrics | ratio of twin to raw velocity on every corrupted message, whether the flow rates, the gyro and the timestamps survive unchanged, whether the other two channels are touched, delivered onset against drawn |
| Data | `data/20260812_square_ATK-004-flow-corruption_s1`, results.csv |

## Notes

The attack scales `vel_body`, `vel_ne` and their filtered counterparts and touches nothing else.
That is a deliberate model rather than a shortcut. `VehicleOpticalFlowVel` carries the flow rates
in rad/s and the velocities in m/s, and what converts one into the other is the range to the
ground. Corrupting the range is a real attack, and it moves the velocities alone.

The alternative, scaling the rates as well, is the one to avoid. PX4 defines
`flow_rate_compensated` as `flow_rate_uncompensated` corrected for angular motion, so the
difference between the two is the gyro contribution. Scaling both leaves that difference
disagreeing with the `gyro_rate` published in the same message, and a consumer subtracting one
from the other would catch the attack for reasons that have nothing to do with cross-checking
independent sensors.

`sensor_optical_flow` carries the raw flow in rad and is relayed untouched, which is consistent
with the same model. `distance_sensor` is not relayed at all, so the flow input the supervision
layer reads is `vehicle_optical_flow_vel`. That boundary is recorded in
[ADR-0004](../../docs/decisions/0004-injector-at-topic-level.md).
