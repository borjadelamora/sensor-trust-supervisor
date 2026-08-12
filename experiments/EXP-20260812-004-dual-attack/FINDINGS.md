# EXP-20260812-004-dual-attack—findings

Both attacks land and both are applied exactly. The third channel stays clean for the whole
flight. The GPS onset arrives 12 ms after its drawn time and the camera onset 3.73 s after its
own, which is the first honest measurement of what the spacing between two channels costs on this
machine.

## Result

**GPS.** 371 fixes pass through identical, then 1083 drift away on bearing 60.00°, tracking the
closed form the scenario declares to 1.58 cm at worst. Every corrupted fix has its own distinct
position and carries a velocity and a course consistent with it, agreeing with the hypotenuse of
its own components to 4.1e-07 m/s, which is float32 resolution. Separation reaches 47.51 m by the
last fix. That is the same profile EXP-20260810-002 describes, re-flown on the shared clock and
unchanged by it, which is what I wanted to know.

**Camera.** 535 frames fall inside the declared window `[24.577, 39.577)` and all 535 were
flooded. 1865 frames outside it are identical to their source, and none came out partly done.

**Flow.** 2400 messages, none altered. The channel the scenario says nothing about is the channel
that has to stay clean, otherwise nothing an attribution result later claims can be trusted.

**The spacing.** Drawn 4.53 s apart, delivered 8.25 s apart. The whole of that difference is one
burst: PX4's stamps jump 4.91 s in this run, and the camera onset at 24.577 s sits inside the
jump, so the first frame that could be flooded arrived once the clock had already reached 28.3 s.
The GPS onset missed a burst and landed within 12 ms.

Checking the rule against every message rather than the first: all 1083 fixes at or past the GPS
onset were shifted and all 371 before it were passed through, all 535 frames inside the camera
window were flooded and all 1865 outside were not. Nothing was missed and nothing went early on
either channel. So the delay is not the rule misfiring, it is that no message existed at the
declared instant to apply it to.

## Numbers

| | value |
|---|---|
| Flight duration | 66.83 s |
| GPS onset, drawn / delivered | 20.047 / 20.059 s |
| Camera onset, drawn / delivered | 24.577 / 28.307 s |
| Onset spacing, drawn / delivered | 4.530 / 8.248 s |
| GPS fixes, raw / twin | 1457 / 1454 |
| Fixes identical before onset | 371 |
| Fixes shifted | 1083 |
| Worst error against the predicted offset | 0.0158 m |
| Separation at the last fix | 47.51 m |
| Offset bearing | 60.00° |
| Ground-speed inconsistency, worst | 4.1e-07 m/s |
| Camera frames inside the window / flooded | 535 / 535 |
| Camera frames outside / altered | 1865 / 0 |
| Flow messages / altered | 2400 / 0 |
| Largest gap in PX4 stamps | 4.91 s |
| Stale stamps ignored | 1 |

## What I would change next time

The delivered spacing is a per-run measurement rather than a property of the seed. The same
scenario at the same seed delivered a different spacing on an earlier flight this afternoon,
because the bursts fall in different places each time. So anything the battery measures against
onset has to use the delivered one out of the bag, not the declared one out of the scenario file,
and the largest stamp gap belongs in every `results.csv` as the thing that explains it.

The bursts are the real finding and they are not specific to this run. They recur every 15 to 19
seconds in all six flights and reach 6.13 s. During one the supervision layer receives almost
nothing at all, so a monitor integrating across a window is going to meet multi-second holes in
every stream. Worth knowing before the first one gets written.

The fix worth trying is the renderer. This machine has `/dev/dxg` and mesa's d3d12 driver
available and the simulation is still rendering in software, which is what puts it below real
time and forces the catching up.
