# EXP-20260812-001-gps-freeze—findings

The freeze holds one position and nothing else moves with it. Across 1133 corrupted fixes the
twin reports exactly one latitude and longitude, zero velocity in all three components, zero
ground speed and zero course.

## Result

401 fixes pass through identical, then the twin stops and stays stopped. The number worth
quoting is the count of distinct positions after onset: one, across 1133 messages. The held fix
is the last clean one, so the twin is not drifting slowly or quantising, it is genuinely frozen.

Every corrupted fix carries zero in `vel_n_m_s`, `vel_e_m_s`, `vel_d_m_s`, `vel_m_s` and
`cog_rad`. Ground speed agrees with the hypotenuse of its own components to exactly zero, which
is trivially true when both are zero and is the point: a receiver claiming to be stationary
should not be publishing a heading. The previous freeze run did, and that was the change this
one exists to record.

Separation between the raw fix and the twin grows to 24.67 m at its widest and reads 14.20 m at
the last fix. That curve is the vehicle flying its square away from the frozen point and part of
the way back rather than anything the attack does, since a frozen fix has no dynamics of its own.

The attack began at 20.651 s against 20.047 s drawn. PX4's stamps arrive in bursts here and the
largest gap in this run is 4.17 s, so there was no fix to corrupt at the declared instant and the
first one after the burst got it. Checking the rule against every message rather than the first
one: all 1133 fixes at or past the onset were held and all 401 before it were passed through, so
nothing was missed and nothing went early.

## Numbers

| | value |
|---|---|
| Flight duration | 63.44 s |
| Drawn onset | 20.047 s |
| Delivered onset | 20.651 s |
| GPS messages, raw / twin | 1534 / 1537 |
| Fixes identical before onset | 401 |
| Fixes held | 1133 |
| Distinct positions after onset | 1 |
| Held fixes with a nonzero velocity field | 0 |
| Held fixes with a nonzero course | 0 |
| Separation, widest | 24.67 m |
| Separation at the last fix | 14.20 m |
| Largest gap in PX4 stamps | 4.17 s |
| Stale stamps ignored | 3 |

## What I would change next time

The separation curve is the most interesting thing here and it is the least measured. A frozen
fix diverges as fast as the vehicle flies, so the shape of that curve is a property of the
mission rather than the attack, and comparing it across missions would say something about how
quickly a physics gate could catch this. That needs the analysis to run from the manifest rather
than by hand.

Nothing here says anything about detection. PX4 navigates on its own sensors and never reads the
twins, so the vehicle flew the square exactly as it would have without the attack.
