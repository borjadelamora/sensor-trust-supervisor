# Plan

Goal: a working, measured demo of fail-operational UAV autonomy in simulation. A supervisor
that notices when a sensor is lying, works out which one, and reconfigures navigation to
keep the mission alive.

## The idea

PX4's EKF fuses GPS/IMU/baro/flow assuming errors are random noise. A spoofed sensor is not
noisy; it lies confidently, and the EKF's own innovation check will not catch a careful spoof
because it never consults an independent source. So the plan is a layer above the autopilot:

1. Fly a scripted offboard mission in SITL (PX4 + Gazebo + ROS 2).
2. Inject attacks at the ROS topic level from a seeded script (GPS freeze/drift/jump,
   optical-flow corruption, camera blackout). Reproducible, and PX4 itself stays untouched.
3. Monitors (C++) cross-check each sensor against independent estimates. Three independent
   velocity estimates (GPS, optical flow, IMU integration) mean two can outvote a liar,
   which lets me attribute the attack rather than just detect it.
4. An arbiter keeps a per-sensor trust state (TRUSTED / SUSPECT / BANNED, with hysteresis).
5. A behaviour tree (BehaviorTree.CPP) reads trust and falls back: GPS nav, then optical-flow
   hold, then inertial dead-reckoning, then controlled landing. Land only when nothing is
   trustworthy.
6. Evaluate with seeded batteries against the stock PX4 failsafe and an unsupervised mission:
   detection latency, a confusion matrix of attacked sensor versus blamed sensor, mission
   outcomes, and the false-positive cost on benign flights.

## Scope fences (so this actually finishes)

- Simulation only, no hardware.
- Attacked sensors: GPS, optical flow, camera. The IMU stays a trusted reference in v1.
- No machine learning. Every check is a physics or consistency rule I can defend line by line.
- PX4's estimator stays; the framework sits above it. That layering is the point.
- The acoustic-detection idea from the spring research is future work, not this project.

## Milestones

| | | |
|---|---|---|
| M0 | Sim environment runs: SITL hovers, sensor topics publish | done |
| M1 | Scripted offboard missions plus a logged set of benign flights | done |
| M2 | Attack injector and three monitors publishing live residuals | done |
| M3 | Trust arbiter and behaviour tree; dual-attack scenario handled; tests green | next |
| M4 | Full evaluation battery, figures, report, demo video | open |

The two early risks are settled: the sim stack runs (on WSL2 in the end, ADR-0002) and
Gazebo's optical flow is good enough to be the third velocity source, though only over
textured ground (ADR-0003). If time runs short I cut attack modes and battery size, never
the three-way voting or the evaluation.
