# Glossary

Terms used in the project.

| Term | Meaning |
|---|---|
| **EKF / EKF2** | Extended Kalman filter. `EKF2` is PX4's state estimator, fusing GPS, IMU, barometer and optical flow into one position and velocity estimate. It assumes sensor errors are random and zero-mean, which is the assumption this project is about. |
| **NED / ENU** | North-east-down and east-north-up. PX4 works in NED, so "up" is a negative number, while ROS conventionally works in ENU. Mixing them silently is the easiest way to fly into the ground. |
| **Offboard mode** | The PX4 mode where setpoints come from an external computer rather than PX4's own mission planner. It stays active only while setpoints keep arriving at 2 Hz or better. |
| **Optical flow** | Horizontal velocity measured by a downward camera watching the ground move, scaled by a range measurement. It needs texture on the ground to work at all. |
| **QoS** | The quality-of-service settings on a ROS 2 topic, mainly reliability and history. A subscriber whose QoS does not match the publisher's receives nothing, with no error. |
| **Relay / twin** | The injector republishes each sensor topic the supervision layer reads under `/quorum/`. That republished copy is the twin, and it is the only version the monitors subscribe to, so an attack is a change made to the twin. |
| **RTF** | Real-time factor: simulated time divided by wall-clock time. 1.0 means the simulation is keeping up. |
| **Scenario file** | A YAML file in `scenarios/` holding a seed and, for each attacked channel, which attack runs and when it starts. The same file and seed replay the same attack. |
| **SITL** | Software in the loop. The real PX4 flight code runs on the laptop against a simulated vehicle, so nothing about the autopilot is mocked. |
| **uORB / XRCE-DDS** | uORB is PX4's internal publish-subscribe bus. The Micro XRCE-DDS agent bridges a chosen list of those topics onto ROS 2 as `/fmu/out/*` and `/fmu/in/*`. |
