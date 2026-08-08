# EXP-20260808-002-gps-freeze—findings

The freeze works. Twenty seconds in, the twin's position stops moving and stays on the fix it
held, while the raw topic carries on around the square.

## Result

The twin held `37.41230540` for the rest of the flight and reached 24.8 m from the true
position at the far corner. It still reported a 3D fix on 10 satellites with a live timestamp,
so the twin reads as a working receiver on a stationary vehicle. Detecting that needs a second
sensor to disagree with it, which is what the monitors are for.

## Numbers

| | |
|---|---|
| Onset in the bag | 17.68 s |
| Messages identical before onset | 373 |
| Peak separation | 24.8 m at t+42.8 s |
| Separation at landing | 14.9 m |
| GPS messages, raw / twin | 1461 / 1445 |

The 16 raw messages with no twin all sit at the head of the bag, before the recorder had
subscribed to the `/quorum` topics. The rate is unchanged once recording is up.

## What I would change next time

The velocities are set to exactly zero, which a monitor could key on instead of the attack
itself. Jitter around zero needs the seeded generator, so it waits for the scenario work.

Onset is hardcoded and measured from the relay's first message, which makes it drift a little
against the bag's own timeline. Anchoring it to something in the mission would be tidier.
