# Setup

How I got PX4 SITL + Gazebo + ROS 2 running, on Windows 11 with WSL2 (Ubuntu 22.04.5).
These are the commands that worked, in order. Everything is pinned to fixed tags, for the
reasons in [ADR-0002](../docs/decisions/0002-dev-environment-and-px4-pin.md).

## Versions

| Component | Ref |
|---|---|
| Ubuntu (WSL2) | 22.04.5 |
| ROS 2 | Humble |
| PX4-Autopilot | tag `v1.17.0` |
| px4_msgs | tag `v1.17.0` |
| Gazebo Sim | 8.14.0 (Harmonic) |
| Micro XRCE-DDS Agent | v3.0.1 |

PX4 and px4_msgs must be on the *same* tag. If they drift apart the topics still tick, but
fields can decode to the wrong values and nothing warns you.

## Prerequisites

Install WSL2 with Ubuntu 22.04 from Windows (`wsl --install -d Ubuntu-22.04`), then inside it:

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y git build-essential cmake python3-pip g++-12
```

`g++-12` is not optional: 22.04 ships GCC 11 by default, which has no `<expected>`, and the
packages here are C++23.

ROS 2 Humble goes in from the ROS apt repository at `packages.ros.org` (`ros-humble-desktop`).
Install it before the PX4 step below. Gazebo Harmonic arrives with the PX4 toolchain script.

## PX4 and Gazebo

```bash
git clone https://github.com/PX4/PX4-Autopilot.git ~/PX4-Autopilot
cd ~/PX4-Autopilot
git checkout v1.17.0
git submodule update --init --recursive
bash ./Tools/setup/ubuntu.sh          # toolchain + gz; re-open the shell afterwards
```

Build and launch the x500 quadcopter in a headless world:

```bash
cd ~/PX4-Autopilot
HEADLESS=1 make px4_sitl gz_x500
```

The first build takes a while. It is up when the PX4 console prints `Ready for takeoff!`.

## Micro XRCE-DDS agent

The bridge between PX4's uORB topics and ROS 2. Second terminal:

```bash
MicroXRCEAgent udp4 -p 8888
```

It starts printing `create_topic` and `create_datawriter` lines as PX4 connects.

## ROS 2 workspace

Third terminal:

```bash
mkdir -p ~/px4_ros_ws/src && cd ~/px4_ros_ws/src
git clone https://github.com/PX4/px4_msgs.git
cd px4_msgs && git checkout v1.17.0
cd ~/px4_ros_ws && colcon build
```

Then in every new shell:

```bash
source /opt/ros/humble/setup.bash
source ~/px4_ros_ws/install/setup.bash
```

Check it:

```bash
ros2 pkg prefix px4_msgs     # prints a path, not "not found"
ros2 topic list              # ~67 topics, including /fmu/out/*
```

## Check it flies

In the PX4 console:

```
param set NAV_DLL_ACT 0
commander takeoff
```

It climbs to 2.5 m and holds. `commander land` brings it back down. The log reads
`Armed by internal command`, `Takeoff detected`, then `Landing detected` and
`Disarmed by landing`.

To see sensor data arriving over the bridge:

```bash
ros2 topic echo /fmu/out/vehicle_gps_position --qos-reliability best_effort --once
```

## Things that caught me out

**`ros2 topic hz` reports nothing on `/fmu/out/*` topics.** In Humble it has no QoS flag, so
it stays on Reliable while PX4 publishes BestEffort. Nothing matches, and it sits at zero
samples with no error at all. `ros2 topic echo` does take `--qos-reliability best_effort`, so
use that and time the messages yourself.

**Arming is refused when headless.** With `NAV_DLL_ACT` above 0, PX4 wants a ground station
heartbeat before it will arm, and headless there isn't one. Setting it to 0 is fine for a hover
test, but it also switches off the datalink-loss failsafe, so it is not a flight default.

**Gazebo survives Ctrl-C.** `make px4_sitl gz_x500` starts the Gazebo server detached, so
stopping PX4 leaves it running. The next launch joins the stale world and fails preflight with
an attitude error. Clear it before relaunching:

```bash
pkill -9 -f '[g]z sim'
pkill -9 -f 'px4_sitl_default/bin/[p]x4'
rm -f /tmp/px4_lock-0 /tmp/px4-sock-0
```

The brackets stop `pkill` matching its own command line.

---

Replayed from a clean shell on 28 July 2026: the workspace and topic checks above pass as
written. The apt and toolchain steps are the ones this machine was built from, checked against
what is actually installed rather than run a second time.
