# EXP-20260810-001-scenario-replay—findings

The scenario file drives the attack. Both flights drew the onset their seed predicted, froze
the twin at that moment and flew the square to a clean landing.

## Result

Seed 1 drew 20.05 s and seed 2 drew 19.05 s, against 20.0473 and 19.0464 from the unit test
that makes the same draw with no simulator. The declared onset is 20 s with 2 s of symmetric
jitter, so the seed is doing exactly the job it was given: it moves the onset within the
declared window and changes nothing else.

The twin behaves as it did in EXP-20260808-002. It holds the last good fix, reports zero
velocity and keeps a 3D fix on 10 satellites, so it still reads as a working receiver
reporting a stationary vehicle.

## Numbers

| | seed 1 | seed 2 |
|---|---|---|
| Drawn onset, from the relay log | 20.05 s | 19.05 s |
| Onset in the bag | 18.17 s | 17.49 s |
| Fixes identical before onset | 523 | 480 |
| Peak separation | 28.7 m at t+37.9 s | 28.6 m at t+37.9 s |
| Separation at the last fix | 20.3 m | 20.4 m |
| GPS messages, raw / twin | 1667 / 1671 | 1605 / 1609 |
| Spawn yaw | 3.2228 rad | 0.1622 rad |

## What the bag onset means

The onset measured in the bag is about 1.6 to 1.9 s earlier than the drawn one, because the
relay comes up before `ros2 bag record` has subscribed and the drawn onset is counted from the
relay's first GPS message. The offset is not the same in both runs, so bag time cannot be used
as attack time. The relay's log line is the reference, and it is the value that belongs in the
ledger. EXP-20260808-002 saw the same thing when the onset was still hardcoded.

The two runs peak within 0.1 m of each other despite spawning 3.06 rad apart, which is what a
freeze should do: the vehicle flies the same square either way, and the separation is set by
how far the mission travels after the onset rather than by the heading.

## Carried forward

Spawn yaw is now drawn from the seed. It was not before this commit: the script read `$RANDOM`
inside a command substitution, and bash reseeds it in a subshell, so every run drew a fresh
heading. Seed 1 gives 3.2228 rad here and gave 2.0477 rad earlier the same evening. The twenty
benign flights predate the fix, so each is described by the yaw in its manifest but none is
regenerable from its seed.

PX4 still navigates on its own sensors and never reads the twins, so neither flight is
disturbed by the attack. That is the injector design working as intended, and it also means
this run says nothing yet about how a vehicle would behave under a real spoof.
