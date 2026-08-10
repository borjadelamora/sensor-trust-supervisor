# EXP-20260810-003-gps-jump—findings

The jump lands as specified: one step of 50 m along bearing 60°, held flat for the rest of the
flight, with the velocity fields untouched.

## Result

455 fixes are identical, then the twin steps away and stays there. Separation across all 1044
corrupted fixes has a minimum of 50.000 m and a maximum of 50.000 m, so the offset really is
constant rather than merely close to it, and the bearing measures 60.00° against the 60°
declared in the scenario file.

The velocity difference between raw and twin is exactly zero in all three components over every
corrupted fix. That is the model rather than an oversight: a constant offset has no derivative,
so a receiver solving Doppler for a trajectory that is the true one shifted by a fixed vector
reports the true velocity. The result is a fix carrying a 50 m position step that its own
velocity cannot account for, which is the inconsistency a kinematic gate exists to catch. It is
the counterpart to ATK-002, which is built to avoid exactly that.

## Numbers

| | value |
|---|---|
| Drawn onset, from the relay log | 20.05 s |
| Divergence in the bag | 18.30 s |
| Fixes identical before onset | 455 |
| Separation, min and max after onset | 50.000 m / 50.000 m |
| Offset bearing | 60.00° |
| Velocity difference, raw against twin | 0.0 m/s in n, e and d |
| GPS messages, raw / twin | 1499 / 1501 |

## One float32 detail

Ground speed differs between raw and twin by up to 4.8e-07 m/s even though the components are
identical. The twin's `vel_m_s` is recomputed from its own components and stored back into a
float32 field, so it lands one bit away from the value the receiver reported. It sits far below
any threshold that will read it, but a jump is not quite a byte-identical message outside the
two position fields.

## What I would change next time

An earlier attempt at this run was discarded: it was interrupted at the world load, and its bag
contained a GPS message from the killed session, stamped on the previous boot clock. The attack
numbers were unaffected but a kept run should not carry a message from a dead session, so it
was re-flown with the ROS daemon stopped first. Batched runs will need that flush between them,
and a timestamp outside the run's own band is a cheap way to detect when it has not happened.
