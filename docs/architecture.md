# Architecture

What the system looks like so far. This is a sketch: the offboard flight node, the attack
injector and the first monitor exist, while the arbiter above them is still a plan. It grows as
things get built.

## Tiers

```
  Gazebo            x500_flow quadcopter, simulated sensors with noise:
     |              GPS, IMU, barometer, magnetometer, downward camera, lidar
     v
  PX4 SITL          EKF2 estimator, flight modes, offboard control
     |
     |              uORB topics bridged to ROS 2 by the Micro XRCE-DDS agent,
     |              camera frames by a second bridge out of Gazebo
     v
  ROS 2 Humble      quorum_offboard: reads a mission file and flies it
     |              quorum_injector: relays the sensor topics, corrupting them on demand
     v
  /quorum/*         what the supervision layer reads
     |
     v
  quorum_monitors   gnss_monitor: a physics gate on the fix, and a velocity residual
     |              against dead reckoning
     v
  (to come)         arbiter and behaviour tree
```

The layering is the point of the project. PX4's estimator stays exactly as it is, and whatever
Quorum adds sits above it rather than inside it.

## Topics

What the offboard node uses, with the rates measured on this machine:

| Topic | Rate | Carries |
|---|---|---|
| `/fmu/out/vehicle_local_position_v1` | 50 Hz | EKF position and velocity in NED, with validity flags |
| `/fmu/out/vehicle_status_v1` | 2 Hz | arming state and current flight mode |
| `/fmu/out/vehicle_attitude` | 50 Hz | orientation quaternion |
| `/fmu/out/vehicle_gps_position` | 30 Hz | GPS fix |
| `/fmu/out/sensor_combined` | 100 Hz | IMU: accelerometer and gyroscope |
| `/fmu/in/offboard_control_mode` | 10 Hz | which setpoint fields PX4 should honour |
| `/fmu/in/trajectory_setpoint` | 10 Hz | the position setpoint being commanded |
| `/fmu/in/vehicle_command` | on demand | arm, and switch to offboard mode |

GPS at 30 Hz is the slowest of the three sensors that matter later, so a cross-check between
GPS and another sensor runs at GPS's rate rather than the faster one's.

The `x500_flow` airframe adds three more, recorded since the flow assessment:

| Topic | Rate | Carries |
|---|---|---|
| `/fmu/out/vehicle_optical_flow_vel` | 45 Hz | flow velocity in m/s, gyro-compensated and scaled by range |
| `/fmu/out/sensor_optical_flow` | 45 Hz | raw flow in rad, with a quality byte |
| `/fmu/out/distance_sensor` | 41 Hz | downward lidar range |

None of those three reach ROS 2 without editing PX4's bridge topic list, and flow only works
over textured ground. Both are covered in [tools/SETUP.md](../tools/SETUP.md) and
[ADR-0003](decisions/0003-textured-world-for-optical-flow.md).

The camera is a Gazebo sensor and reaches ROS 2 through a bridge of its own, `ros_gz_bridge`
alongside the XRCE-DDS agent:

| Topic | Rate | Carries |
|---|---|---|
| `/camera/flow_image` | 45 Hz | 100×100 `rgb8` frames, straight out of Gazebo |

## The relay

Everything the supervision layer reads arrives on a `/quorum/*` twin. `quorum_injector`
subscribes to each source, applies whatever the scenario file asks of it, and republishes under
the new name. Given no scenario it republishes untouched.

| Source | Twin |
|---|---|
| `/fmu/out/vehicle_gps_position` | `/quorum/vehicle_gps_position` |
| `/fmu/out/sensor_combined` | `/quorum/sensor_combined` |
| `/fmu/out/sensor_optical_flow` | `/quorum/sensor_optical_flow` |
| `/fmu/out/vehicle_optical_flow_vel` | `/quorum/vehicle_optical_flow_vel` |
| `/fmu/out/vehicle_attitude` | `/quorum/vehicle_attitude` |
| `/fmu/out/distance_sensor` | `/quorum/distance_sensor` |
| `/camera/flow_image` | `/quorum/flow_image` |

PX4 navigates on the sources, and the twins exist for the supervision layer alone. That keeps
the autopilot as it ships and every run reproducible from a seed, and it makes the reach of an
attack precise: it changes what the supervision layer believes, and arrives at the vehicle
through whatever navigates on the twins. Currently the mission flies from the sources, so an
attacked flight follows the same square as a clean one.

The IMU is the reference the other two channels get checked against, so `sensor_combined` is
relayed as it comes and v1 trusts it. `vehicle_attitude` is relayed on the same terms: the
accelerations are in the body frame, and the quaternion is what turns them into NED. It comes
out of EKF2 rather than off a sensor, so it is not as independent of GPS as the raw IMU is.

Attack onsets are counted in PX4's clock, from the first stamp the relay sees, so all three
channels are timed against one reference.

## Frames

PX4 works in NED and ROS conventionally works in ENU. The mission files and the node both speak
NED, so there is one boundary rather than conversions scattered through the code. If ENU ever
enters, it converts in one named function and nowhere else.
