# Recorded flights

One directory per run, named `YYYYMMDD_<mission>_<scenario>_s<seed>`, written by
[`tools/record_flight.sh`](../tools/record_flight.sh). `<scenario>` is `none` when the run
carries no attack.

The bag payload (`*.db3`) is not tracked. A run is reproduced by checking out the commit in its
manifest and rerunning the script with the same mission, scenario and seed.

`relay.log` is what the injector printed: the attack it planned, the PX4 stamp it counted onsets
from and the onset drawn for each channel. Every onset is relative to that stamp, so it is the
file an experiment entry quotes them against. Runs recorded before 12 August predate it.

`monitor.log` is what the GNSS monitor printed: the fixes and IMU samples it turned away, and
the reasons it re-anchored. Runs recorded before 15 August predate it. From 16 August a second
monitor reads the flow, so the two logs are `gnss_monitor.log` and `flow_monitor.log`, and the
flow one also records every trust level it changed to.

Manifest fields:

| Field | Meaning |
|---|---|
| `seed` | Seeds the spawn heading, and overrides the seed the scenario file declares. |
| `scenario` | The attack scenario flown, or `none` for a plain relay. |
| `home_yaw_rad` | Spawn heading, drawn from the seed. This is what varies the flight between seeds. |
| `model`, `world` | The airframe and Gazebo world. The world places the vehicle and fixes the global origin. |
| `commit`, `px4_version` | The code that flew it. |
| `exit_code` | 0 flew and landed, 1 bad arguments, 2 the stack never came up, 124 the flight overran. |

Gazebo physics and sensor noise are not seeded, so two runs at the same seed are close but not
identical.

`20260807_square_s1` … `s20` are the benign baseline: same mission, twenty spawn headings, on
`gz_x500_flow` in the baylands world. They were flown before the scenario segment entered the
name, so they carry the older `YYYYMMDD_<mission>_s<seed>` form. They also predate the fix to
the spawn heading draw: the script read `$RANDOM` inside a command substitution, and bash
reseeds it in a subshell, so those twenty headings did vary but not from their seeds. Each one
is recorded in its manifest, so the runs are still described. Rerunning them at the same seed
now produces a different heading. An earlier set of twenty flew the same mission on
`gz_x500` in the default world, which has no flow sensor and no ground texture for one to work
against ([ADR-0003](../docs/decisions/0003-textured-world-for-optical-flow.md)), so it was
replaced rather than kept.
