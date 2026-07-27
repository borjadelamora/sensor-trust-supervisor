# sensor-trust-supervisor

Supervisory layer for a simulated quadcopter that keeps the mission going when sensors fail
or get attacked: GPS spoofing, optical-flow tampering, camera blackout. The core idea is to
keep an explicit per-sensor trust state by cross-checking sensors against independent
estimates of the same quantity, and to use a behaviour tree to fall back onto whatever
navigation source is still trustworthy, instead of blindly trusting the EKF.

Grew out of my Spring 2026 research on UAV sensor attacks.

**Status:** just starting. PX4 SITL + Gazebo + ROS 2 now run on this machine, a Windows
laptop under WSL2 (see ADR-0002); next job is flying it from my own code.

Planned stack: Ubuntu 22.04 (WSL2) · ROS 2 Humble · Gazebo · PX4 SITL · BehaviorTree.CPP.
C++ for the framework itself, Python for attack injection and analysis.

Rough plan and milestones: [PLAN.md](PLAN.md).

MIT licence.
