# EXP-20260815-001-gnss-velocity-residual—findings

The residual separates the freeze from a benign flight and leaves the drift inside it. The
freeze peaks at 5.579 m/s against a benign 1.104 m/s, and the drift peaks at 1.099 m/s.

## Result

The freeze crosses the 1.40 m/s ceiling 30 ms after the spoof lands. A frozen fix reports zero
velocity while the vehicle flies, so the reference integrates away from every new anchor and
the disagreement keeps regrowing.

The drift answers its onset, 0.490 to 1.099 m/s, then gives nothing, because the next anchor
takes the spoofed velocity as its starting point. That peak lands 5 mm/s below the benign
flight's.

## Numbers

| | value |
|---|---|
| Threshold, fixed beforehand | 1.40 m/s |
| Benign peak | 1.104 m/s |
| Freeze peak, before onset | 5.579 m/s, 0.325 m/s |
| Freeze onset to crossing | 0.030 s |
| Drift peak, before onset | 1.099 m/s, 0.490 m/s |
| Crossings, benign / drift | 0 / 0 |

## What I would change next time

One benign run is thin as a control for the ceiling. It is dead reckoning drift in the descent,
and three runs of this seed have peaked at 0.556, 1.398 and 1.104 m/s, so a threshold worth
committing to needs the full benign dataset behind it and an ADR to record it.
