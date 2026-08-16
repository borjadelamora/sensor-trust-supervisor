# EXP-20260816-001-flow-velocity-residual—findings

The peak residual separates nothing and the debounce separates everything. Benign peaks at
2.525 m/s and ATK-007 at 2.483 m/s, so on peak alone the benign flight looks worse than the
attack. Sustained above the threshold, benign never gets there and both attacks do.

## Result

Benign spends 5% of its samples above 1.0 m/s and never 2 s in a row, so it raises nothing. The
transients are takeoff and descent, where the dead-reckoned reference is worst, and they are
short. ATK-007's whole-flight maximum lands before its own onset for the same reason.

Both attacks are caught late. ATK-004 takes 17.4 s from the delivered onset, ATK-007 33.6 s.
Both ramp their scale over 10 s, so the residual grows slowly, and the reference re-anchors on
the flow it is being fed while that happens. The weaker attack is the slower one: 0.5 scale
takes 17 s, 0.7 scale takes 34 s.

ATK-004 flips back to TRUSTED and then to SUSPECT again. The anchor expires mid-attack and
re-anchors on the corrupted velocity, which erases the disagreement until the reference has
drifted far enough to rebuild it. This is the same mechanism EXP-20260815-001 found on GNSS,
reproduced on a second channel and a different attack.

## Numbers

| | none | ATK-004 | ATK-007 |
|---|---|---|---|
| Peak residual | 2.525 | 3.771 | 2.483 |
| Median | 0.474 | 0.476 | 0.594 |
| 95th percentile | 1.015 | 2.996 | 1.990 |
| Before onset | | 3.303 | 2.483 |
| Onset to SUSPECT | | 17.430 | 33.604 |
| SUSPECT episodes | 0 | 2 | 1 |

Threshold 1.0 m/s held for 2.0 s, both provisional.

## What I would change next time

One benign run is not a false-positive rate. An earlier benign flight on this same seed, flown
an hour before at the same threshold, did raise a SUSPECT at 1.09 m/s, so the margin here is
thinner than a single clean run suggests. The benign 95th percentile is 1.015 m/s against a
1.0 m/s threshold, which is no margin at all.

Latency is the number the report will be asked about and it is poor. Separating how much of it
is the attack ramping from how much is the reference re-anchoring needs a step-change flow
attack, which no scenario currently defines.
