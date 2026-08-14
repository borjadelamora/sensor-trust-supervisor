# EXP-20260814-001-strength-and-the-range—findings

Every attack reaches the magnitude its scenario declares wherever the sampling allows, and both
exceptions are burst behaviour. Thirteen of fourteen onsets land within 32 ms of the draw and the
benign flight is identical to its source on all five channels.

## Result

**Onsets.** Delivered onsets sit 3 ms to 32 ms after the drawn ones on thirteen of the fourteen
attacked channels. The fourteenth is ATK-006's camera at 1.857 s late, a stamp gap falling across
the declared instant.

**GPS.** The jump reaches 50.000 m. The drift reaches 47.000 m on ATK-007 and 14.000 m on ATK-008
against peak velocity biases of exactly 1.000 m/s and 0.300 m/s, and both close their rise inside
the flight with the bias back at 0.000 m/s over the new ease. The oscillating spoof peaks at
24.000 m against a declared 20 m, and its signed ground-speed change runs from −0.654 to +1.520 m/s, so the fix pulls back as well as pushing out.

**Flow and the range.** ATK-004 scales the reported velocity to 0.500 and the reported range to
0.500. ATK-007 scales both to 0.700. The raw topic passes through untouched in both, since this
airframe leaves `distance_m` out.

**The lens.** ATK-005 and ATK-006 flood to level 0 and take the flow with them, velocity and raw
pixel flow both to 0.000. ATK-011 floods the other way to 255, past the 240 a frame-mean monitor
is written against, with 258 of its 411 frames caught part way there. `flow_rate_compensated`
equals `flow_rate_uncompensated` minus `gyro_rate` throughout.

**Channels no scenario names.** Untouched throughout, on every run that leaves them alone.

## Numbers

| | value |
|---|---|
| Runs | 12, all exit code 0, commit 5744707 |
| Benign run, corrupted messages | 0 of 11 598 |
| ATK-007 / ATK-008 peak separation | 47.000 / 14.000 m |
| ATK-002 peak separation | 46.794 m, rise unfinished at landing |
| ATK-009 peak separation | 16.811 m, window truncated by a burst |
| ATK-010 peak separation | 24.000 m against a declared 20 m |
| ATK-004 velocity and range ratio | 0.500 and 0.500 |
| ATK-007 velocity and range ratio | 0.700 and 0.700 |
| Raw flow altered under a flow attack | 0 of 2371, 0 of 2334 |
| ATK-011 frames caught mid-ramp | 258 of 411, 252 distinct levels |
| Gyro relation in the twin, worst | 1.19e-07 over 5195 messages |
| Largest px4 stamp gap | 4.98 s to 7.97 s per run |

## What I would change next time

ATK-002's rise ends 1.0 s after the flight does, so it lands at 46.794 m and its closing ease only
half runs. The rise was sized against a 70 s flight and these came in nearer 65 s.

The bursts grew, reaching 7.97 s here against 6.13 s on 12 Aug, long enough to swallow a whole
ramp. ATK-011's 8 s rise survived because the burst fell late, but ATK-009's spoof lost the tail
of its window and never reached 20 m. Any shape meant to be observed part way through has to be
long against the burst.

`flooded_level` on a freeze reports the replayed frame's brightness, 79.11 on ATK-010, which
measures staleness. A freeze deserves a metric of its own.
