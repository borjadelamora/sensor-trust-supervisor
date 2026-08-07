# Recorded flights

One directory per run, named `YYYYMMDD_<mission>_s<seed>`, written by
[`tools/record_flight.sh`](../tools/record_flight.sh).

The bag payload (`*.db3`) is not tracked. A run is reproduced by checking out the commit in its
manifest and rerunning the script with the same mission and seed.

Manifest fields:

| Field | Meaning |
|---|---|
| `seed` | Seeds the spawn heading. The only input the run needs to repeat. |
| `home_yaw_rad` | Spawn heading, drawn from the seed. This is what varies the flight between seeds. |
| `model`, `world` | The airframe and Gazebo world. The world places the vehicle and fixes the global origin. |
| `commit`, `px4_version` | The code that flew it. |
| `exit_code` | 0 flew and landed, 1 bad arguments, 2 the stack never came up, 124 the flight overran. |

Gazebo physics and sensor noise are not seeded, so two runs at the same seed are close but not
identical.

`20260807_square_s1` … `s20` are the benign baseline: same mission, twenty spawn headings, on
`gz_x500_flow` in the baylands world. An earlier set of twenty flew the same mission on
`gz_x500` in the default world, which has no flow sensor and no ground texture for one to work
against ([ADR-0003](../docs/decisions/0003-textured-world-for-optical-flow.md)), so it was
replaced rather than kept.
