# EXP-20260818-001-camera-monitor—findings

The camera monitor flips under both lens attacks and raises nothing on the benign flight. GNSS
stayed trusted throughout, which EXP-20260817-001 could not manage.

## Result

Blackout is a step, so it crosses the band on the first corrupted frame and the level follows
0.500 s later. That is the sustain period, so the residual adds no delay of its own.

Dazzle takes 2.580 s from the onset. The ramp spends 2.080 s of that climbing to the band edge
and the sustain takes the rest. The stage wanted a flip within a second of the onset and only
blackout meets it. From the band crossing both attacks take 0.500 s, and that is the part the
monitor is answerable for.

Benign peaked at 27.748 against a threshold of 55.0. The band was fixed before these runs from
the four benign bags of 14 to 17 August, which put the worst benign departure at 27.7, so the
flight landed on the pre-registered number. Both attacks recovered 0.520 s after the last
corrupted frame.

## Flow follows the camera

Flow went SUSPECT in both camera runs, and its own sensor was corrupted: 414 corrupted flow
messages under ATK-005 and 572 under ATK-011, because the injector scales flow by the lens factor
while a camera episode is active. One lens feeds both, so its verdict is right.

GPS carried 0 corrupted messages in all three runs and GNSS never left TRUSTED. So on the reading
that no monitor with an untouched sensor should fire, the M2 checkpoint holds.

## Numbers

| | benign | ATK-005 | ATK-011 |
|---|---|---|---|
| Camera peak / episodes | 27.748 / 0 | 105.000 / 1 | 150.000 / 1 |
| GNSS peak / episodes | 1.058 / 0 | 1.330 / 0 | 1.003 / 0 |
| Flow peak / episodes | 4.431 / 0 | 5.667 / 2 | 5.478 / 1 |
| Onset to band crossing | | 0.000 | 2.080 |
| Onset to first SUSPECT | | 0.500 | 2.580 |

Camera at 105 ± 55 pixel levels held for 0.5 s. GNSS and flow unchanged from EXP-20260817-001.

## What I would change next time

Every bag with camera frames in it was flown on seed 1, so the band has seen one yaw over
baylands. The battery is the first thing that will test whether 77.79 to 133.21 generalises.

A mean over the whole frame cannot see `CameraFreeze`, because a held frame keeps its mean.
ATK-010 will not flip this monitor and the report should say so.

Flow took 12.659 s and 4.884 s to notice attacks that corrupted it, against 0.500 s for the
camera. The cheapest monitor is the fastest one here.
