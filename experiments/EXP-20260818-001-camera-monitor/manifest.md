# EXP-20260818-001-camera-monitor

| Field | Entry |
|-------|-------|
| Experiment id | EXP-20260818-001-camera-monitor |
| Date | 2026-08-18 |
| Goal | Whether the frame mean and its debounce separate a blacked-out or dazzled lens from a benign flight |
| Commit | 73986c9 |
| Seed(s) | 1 |
| Mission | square |
| Model / world | gz_x500_flow / baylands |
| Attack | none (benign), camera blackout (ATK-005), camera dazzle (ATK-011) |
| Attack onset | 20 s ± 2 s jitter for both, from the relay's first px4 stamp |
| Attack parameters | ATK-005 step, intensity 1.0, 15 s. ATK-011 intensity 1.0, linear rise 8 s, ease 2 s, 15 s |
| Metrics | Frame mean per run, in pixel levels, against a band of 105 ± 55 held for 0.5 s. Latency runs from the delivered onset to the first SUSPECT, and from the first frame outside the band. |
| Repetitions | one run per scenario |
| Data | `data/20260818_square_{none,ATK-005-camera-blackout,ATK-011-camera-dazzle}_s1`, results.csv |

## Notes

The band is pre-registered from the four benign flow bags of 14 to 17 August: 10 168 frames with
means from 77.79 to 133.21, bimodal by altitude at about 78 in cruise and about 120 near the
ground. The centre is the midpoint of that range and the half-width is twice the widest departure
from it. All four bags are seed 1, so the band has seen one yaw over baylands.

ATK-011 ramps over 8 s, so its onset-to-flip time carries the ramp as well as the sustain period,
which is why latency is measured from the band crossing too. ATK-005 blacks out the lens the flow
sensor shares, so flow is expected to follow the camera in that run.
