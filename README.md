# sensor-trust-supervisor

Supervisory layer for a simulated quadcopter that keeps the mission going when sensors fail
or get attacked: GPS spoofing, optical-flow tampering, camera blackout. The core idea is to
keep an explicit per-sensor trust state by cross-checking sensors against independent
estimates of the same quantity, and to use a behaviour tree to fall back onto whatever
navigation source is still trustworthy, instead of blindly trusting the EKF.

Grew out of my Spring 2026 research on UAV sensor attacks.

**Status:** the scripted mission flies, the injector replays eleven seeded attack scenarios
and three monitors (GNSS, optical flow, camera) publish live trust reports on a common
interface. Next is the arbiter that turns those reports into a per-sensor trust state. Kept
runs live in [experiments/](experiments/), the current shape of the system in
[docs/architecture.md](docs/architecture.md).

Stack: Ubuntu 22.04 (WSL2) · ROS 2 Humble · Gazebo Harmonic · PX4 SITL, with
BehaviorTree.CPP to come for the supervisor. C++ for the framework itself, Python for attack
injection and analysis. Versions are pinned in [tools/SETUP.md](tools/SETUP.md).

Rough plan and milestones: [PLAN.md](PLAN.md).

MIT licence.
