# EXP-20260810-002-gps-drift—findings

The drift lands as specified. The twin leaves the raw fix from zero rather than stepping off
it, settles onto 1 m/s along bearing 60°, and its velocity fields stay consistent with the
position they imply for the whole run.

## Result

491 fixes are identical, then the separation grows away from zero. The first corrupted fix
differs by less than a centimetre, which is the ramp doing its job: a synchronous spoofer takes
over without a discontinuity, so both the offset and the velocity bias start at nothing.

Against the closed form the scenario file declares, ½·(rate/ramp)·t² inside the ramp and
rate·(t − ramp/2) after it, the worst disagreement across 1040 corrupted fixes is 2.8 cm and
the mean is 2.6 cm. That is the resolution of the comparison rather than a real error: the
separation is computed from latitude and longitude in a spherical approximation, and the
prediction uses the drawn onset, which is only known to the two decimals the relay logs.

The velocity bias after the ramp averages 0.5000 north and 0.8660 east, magnitude 1.0000 m/s
on a bearing of 60.00°, which is the declared rate resolved along the declared bearing to four
decimals.

## Numbers

| | value |
|---|---|
| Drawn onset, from the relay log | 20.05 s |
| Divergence in the bag | 18.06 s |
| Fixes identical before onset | 491 |
| Separation at the last fix | 41.19 m at t+61.7 s |
| Worst error against the predicted offset | 0.028 m |
| Velocity bias after the ramp | 1.0000 m/s at 60.00° |
| GPS messages, raw / twin | 1531 / 1532 |

## Coherence of the corrupted fix

Ground speed and course over ground are not independent of the velocity components: they are
the same horizontal vector in polar form. Across every corrupted fix, the twin's `vel_m_s`
agrees with the hypotenuse of its own components to 4.3e-07 m/s, which is float32 resolution.
A consumer reading either pair sees the same spoofed motion, which is the point. Leaving the
derived fields alone would have left the true track visible in a message claiming otherwise.

## What I would change next time

The comparison against the predicted offset lives in a throwaway script, and the drawn onset
comes off a log line read by eye. Both belong in the recording rather than being reconstructed
afterwards: the relay could write the onset it drew alongside the bag, and the raw-against-twin
comparison should be a script that runs from the manifest alone.

Nothing here says anything about detection. PX4 navigates on its own sensors and never reads
the twins, so the vehicle flew the square exactly as it would have without the attack. The
first run where this drift changes an outcome is the one where a monitor is reading these
topics.
