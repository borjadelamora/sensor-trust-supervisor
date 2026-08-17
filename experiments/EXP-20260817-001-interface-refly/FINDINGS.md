# EXP-20260817-001-interface-refly—findings

Flow came through the extraction with its numbers intact, GNSS detects the freeze with the level
it gained today and each monitor also went SUSPECT during the attack on the other channel.

## Result

Flow under ATK-004 raises two SUSPECT episodes, the first 17.840 s after the delivered onset,
against two at 17.430 s in EXP-20260816-001 on this seed.

GNSS under ATK-001 raises three episodes, the first 2.048 s after the onset, from a pre-onset
maximum of 0.221 m/s to a peak of 6.526. Two seconds of that latency is the sustain period, so
the residual crosses within 50 ms, close to the 0.030 s EXP-20260815-001 measured on the raw
signal.

Benign raises nothing on either channel, though flow peaks at 7.059 m/s there against a 1.0 m/s
threshold, which the debounce absorbs on its own.

## The cross-channel episodes

`analyse_run.py` reports 0 corrupted flow messages in the ATK-001 run and 0 corrupted gps
messages in the ATK-004 run, so neither monitor was reading a spoofed sensor when it fired.

A SUSPECT says the sensor and the reference it is checked against disagree. It does not say
which of the two moved, and in these two episodes it was the reference. Both monitors dead
reckon from the same imu, so a stretch of flight that drifts the reference far enough for long
enough reads as a fault on whichever channel is being compared to it at the time.

## Numbers

| | benign | ATK-001 | ATK-004 |
|---|---|---|---|
| GNSS peak / episodes | 0.497 / 0 | 6.526 / 3 | 2.601 / 1 |
| Flow peak / episodes | 7.059 / 0 | 5.624 / 1 | 4.505 / 2 |
| Onset to first SUSPECT | | 2.048 | 17.840 |

GNSS at 2.0 m/s held for 2.0 s, flow at 1.0 m/s held for 2.0 s, both provisional.

## What I would change next time

Two false positives out of three flights is a rate I cannot put a number on, and the benign
dataset is sitting there to put one on it. Flow's 7.059 m/s benign peak is where I would start:
it passes only because it is brief and I have never measured how long a drift excursion lasts.
