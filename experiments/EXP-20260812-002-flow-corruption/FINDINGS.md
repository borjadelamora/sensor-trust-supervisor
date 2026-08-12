# EXP-20260812-002-flow-corruption—findings

The flow channel reads half its true speed from onset, and every field the velocity is computed
from arrives untouched. 645 messages pass through identical, then 1861 are scaled.

## Result

The ratio of twin to raw velocity is 0.5000 on every corrupted message and on every component of
`vel_body`, `vel_ne` and both filtered pairs. Not close to a half, exactly a half: the scale is a
float32 multiply and the ratio comes back as the same float every time.

`flow_rate_uncompensated`, `flow_rate_compensated`, `gyro_rate`, `timestamp` and
`timestamp_sample` are byte-identical between raw and twin across all 1861 corrupted messages.
That is the coherence the attack is built around. The rates are in rad/s and it is the range they
are scaled by that this attack corrupts, so leaving them alone is what the model requires, and it
keeps the gyro term implied by the difference between the two rates agreeing with the gyro
published beside them.

The other two channels are untouched: none of the 1519 matched GPS fixes differs from its source
and none of the 2510 camera frames was flooded. That matters more than it sounds, because the
scenario file declares one attacked channel and the relay has to be a no-op on the rest for any
later attribution result to mean anything.

The attack began at 24.467 s against 21.802 s drawn, 2.67 s late. PX4's stamps arrive in bursts
here and the largest gap in this run is 4.40 s, so no flow message existed at the declared
instant and the first one after the burst was scaled. Checked against every message rather than
the first: all 1861 at or past the onset were scaled and all 645 before it were left alone.

## Numbers

| | value |
|---|---|
| Flight duration | 66.69 s |
| Drawn onset | 21.802 s |
| Delivered onset | 24.467 s |
| Flow messages, twin | 2510 |
| Messages identical before onset | 645 |
| Messages scaled | 1861 |
| Velocity ratio, twin over raw | 0.5000 |
| Corrupted messages with an altered flow rate | 0 |
| Corrupted messages with an altered gyro or timestamp | 0 |
| GPS fixes altered | 0 |
| Camera frames altered | 0 |
| Largest gap in PX4 stamps | 4.40 s |
| Stale stamps ignored | 2 |

## What I would change next time

A constant scale is the easy case, in the same way the GPS jump is. Reading half speed for
the rest of the flight is a large and unvarying disagreement with an IMU-integrated estimate, and
a monitor that cannot catch it is not worth having. The interesting flow scenarios are a scale
close to one or a scale that ramps, and both belong in the battery once there is a monitor to run
them against. The scenario file should say which sort it is, the way the GPS files do.

There is also no measurement here of what the corrupted velocity does to a residual, because
nothing is computing one yet.
