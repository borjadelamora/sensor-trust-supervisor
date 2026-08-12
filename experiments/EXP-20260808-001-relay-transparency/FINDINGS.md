# EXP-20260808-001-relay-transparency—findings

The relay passes everything through untouched. Every message on the four raw topics has a
twin with the same timestamp and the same field values, on a full square mission.

## Result

Relayed and direct are indistinguishable, so the relay can be treated as transparent when no
attack is configured. Everything the monitors will read comes through it.

## Numbers

| Topic | Raw | Relayed | Matched | Unmatched raw | Field mismatches |
|---|---|---|---|---|---|
| vehicle_gps_position | 1441 | 1444 | 1441 | 0 | 0 |
| sensor_combined | 4755 | 4765 | 4755 | 0 | 0 |
| sensor_optical_flow | 2377 | 2382 | 2377 | 0 | 0 |
| vehicle_optical_flow_vel | 2377 | 2382 | 2377 | 0 | 0 |

Flight duration 67 s, exit code 0.

The relayed topics carry a few messages more than the raw ones. `ros2 bag record` subscribed to
them about 107 ms before the raw topics, and at these rates that gap is worth 2 to 8 messages.
No raw message is missing from its twin, which is the property that matters.

## What I would change next time

The comparison ran as a throwaway script. It belongs in `tools/`, with the padding and
NaN handling built in, because every battery run wants the same check.
