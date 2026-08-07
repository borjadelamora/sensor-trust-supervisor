# Recorded flights

One directory per run, named `YYYYMMDD_<mission>_s<seed>`, written by
[`tools/record_flight.sh`](../tools/record_flight.sh).

The bag payload (`*.db3`) is not tracked. A run is reproduced by checking out the commit in its
manifest and rerunning the script with the same mission and seed.

Manifest fields:

| Field | Meaning |
|---|---|
| `seed` | Seeds the spawn draws below. The only input the run needs to repeat. |
| `home_lat`, `home_lon` | Global origin. Shifts where the flight happens, not the local NED path. |
| `home_yaw_rad` | Spawn heading. This is what varies the flight between seeds. |
| `commit`, `px4_version` | The code that flew it. |
| `exit_code` | 0 flew and landed, 1 bad arguments, 2 the stack never came up, 124 the flight overran. |

Gazebo physics and sensor noise are not seeded, so two runs at the same seed are close but not
identical.

`20260807_square_s1` … `s20` are the benign baseline: same mission, twenty spawn headings.
