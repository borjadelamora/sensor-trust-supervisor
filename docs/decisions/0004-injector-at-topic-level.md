# ADR-0004: Injecting attacks at the ROS topic level

## Status
Accepted, 12/08/2026

## Context
The project needs sensor attacks that repeat exactly from a seed, on an autopilot that behaves as it would in the field. There were three places to put them.

PX4 injects failures itself through the `failure` command. It reaches the real driver path, but what it offers are failures rather than spoofs: off, stuck, garbage. A 1 m/s drift along a chosen bearing means patching PX4, and a forked autopilot in the loop costs me the claim that the vehicle under test is the shipped one.

A Gazebo sensor plugin corrupts the measurement before PX4 reads it, so the EKF fuses the lie and the vehicle flies wrong. That is the most realistic of the three, but it puts the attack inside the physics step and its parameters in SDF, so a run repeats only as far as the simulation does.

A relay in ROS 2 sits between the sensor topics and everything that reads them, which makes the corruption a function of the message, the scenario and the seed.

## Decision
Attacks live in a Python relay, `quorum_injector`. It subscribes to each sensor topic the supervision layer reads, republishes it under `/quorum/` and corrupts it on the way through when a scenario says so. Monitors subscribe to the twins and nothing else. The scenario is a YAML file in `scenarios/` carrying a declared seed, and one `numpy` Generator built from that seed makes every draw in the run.

Onset is the declared time plus a symmetric jitter, so twenty seeds strike at twenty points of the square rather than all at the same one. Every channel draws whether it is attacked or not, so adding a camera attack to a file leaves the GPS onsets of its existing seeds where they were. Elapsed time counts from the first message the relay sees on that stream.

A corrupted message has to hold together, or a consistency check catches it for reasons that say nothing about the idea being tested. The frozen fix holds position and velocity and reports a zero course, since a receiver claiming to be stationary should not still publish the true heading. The drifting fix reports the velocity that is the derivative of its own offset, with ground speed and course following from the components. Flow corruption scales the velocity fields and leaves the rates in rad/s alone, because what it models as corrupted is the range those rates are scaled by. Camera blackout floods every frame in the window to one level.

The jump is the exception, deliberately. A constant offset has no derivative, so the position steps while the velocity passes through untouched and the fix carries a step its own velocity cannot account for. That is the easy case, and what a kinematic gate is for.

## Consequences
PX4 stays as it ships and its estimator never fuses a spoof, so an attacked flight and a clean one fly the same square. What the attack degrades is the supervision layer's view, and it reaches the vehicle's behaviour only through whatever navigates on the twins. The baselines in the evaluation have to be written on that basis: the stock failsafe comparison is against an autopilot that was never lied to, and that belongs in the report.

`distance_sensor` is not relayed, and the flow input the supervision layer reads is `vehicle_optical_flow_vel`. Raw flow and range together reconstruct the true velocity, so a monitor reading both would see through the attack for reasons that say nothing about cross-checking independent sensors.

Zeroing the course on a frozen fix changed behaviour that EXP-20260808-002 and EXP-20260810-001 describe. Their numbers stand for the code they name, and the freeze runs get re-flown before any are quoted for detection.

Every channel is timed by one clock, PX4's, counting from the first PX4 stamp the relay sees. Camera frames carry no stamp of their own, so they take the most recent one, which `sensor_combined` keeps fresh to within 10 ms. Timing each stream by the clock its own messages carry is the obvious alternative and it does not hold: over a 70 s run PX4's stamps and the node's wall clock wander ±3 s apart and back, so a spacing declared between two channels would not survive the flight. The cost of the choice is that onset resolution follows stamp arrival. This host runs the simulation below real time and then catches up in bursts, and most of what PX4 emits during one is dropped, so the clock can cross a declared onset with no message in between to corrupt. Every message that does arrive is corrupted exactly when the rule says, but the first corrupted one has landed up to 3.7 s after the declared onset, so it is the delivered onset a result rests on rather than the declared one.

Onset is counted from relay start rather than from takeoff, so a fixed seed attacks at the same second of the relay's life rather than the same point of the trajectory. The drawn onset is in the relay's log and belongs in the run manifest before the battery runs.
