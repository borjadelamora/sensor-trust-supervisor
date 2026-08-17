#!/usr/bin/env bash
set -e

if [ "$#" -lt 2 ] || [ "$#" -gt 4 ]; then
  echo "Usage: $0 <mission_name> <seed> [px4_model] [scenario_file]"
  exit 1
fi

MISSION=$1
SEED=$2
MODEL=${3:-gz_x500_flow}
SCENARIO=${4:-}

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
MISSION_FILE="$REPO/src/quorum_offboard/config/missions/${MISSION}.yaml"

SCENARIO_NAME=none
if [ -n "$SCENARIO" ]; then
  case $SCENARIO in
    /*) ;;
    *) SCENARIO="$REPO/$SCENARIO" ;;
  esac
  SCENARIO_NAME=$(basename "$SCENARIO" .yaml)
fi

RUN="$(date +%Y%m%d)_${MISSION}_${SCENARIO_NAME}_s${SEED}"
BAG_DIR="$REPO/data/$RUN"

# Baylands is a large world and can take a couple of minutes to load
SIM_TIMEOUT_S=300
FLIGHT_TIMEOUT_S=300

# The flow camera reaches ROS through a second bridge, see SETUP.md
CAMERA_TOPIC=/camera/flow_image
CAMERA_TWIN=/quorum/flow_image

# Exit status: 1 bad arguments, 2 stack never came up, 124 flight overran, else the node's.
if [ ! -f "$MISSION_FILE" ]; then
  echo "No mission file at $MISSION_FILE"
  exit 1
fi

if [ -n "$SCENARIO" ] && [ ! -f "$SCENARIO" ]; then
  echo "No scenario file at $SCENARIO"
  exit 1
fi

echo "Run $RUN"

# Background processes inherit the ROS environment
source ~/px4_ros_ws/install/setup.bash
source "$REPO/install/setup.bash"

RECORDER_PID=""
PX4_PID=""
AGENT_PID=""
RELAY_PID=""
BRIDGE_PID=""
GNSS_MONITOR_PID=""
FLOW_MONITOR_PID=""

kill_stack() {
  pkill -9 -f ninja || true
  kill -9 "$PX4_PID" 2>/dev/null || true
  kill -9 "$AGENT_PID" 2>/dev/null || true
  kill -9 "$RELAY_PID" 2>/dev/null || true
  kill -9 "$BRIDGE_PID" 2>/dev/null || true
  kill -9 "$GNSS_MONITOR_PID" 2>/dev/null || true
  kill -9 "$FLOW_MONITOR_PID" 2>/dev/null || true
  pkill -9 -f MicroXRCEAgent || true
  pkill -9 -f relay_node || true
  pkill -9 -f gnss_monitor_node || true
  pkill -9 -f flow_monitor_node || true
  pkill -9 -f parameter_bridge || true
  pkill -9 -f '[g]z sim' || true
  pkill -9 -f 'px4_sitl_default/bin/[p]x4' || true
}

# SIGINT closes the bag cleanly
stop_recorder() {
  if [ -n "$RECORDER_PID" ]; then
    kill -INT "$RECORDER_PID" 2>/dev/null || true
    wait "$RECORDER_PID" 2>/dev/null || true
    RECORDER_PID=""
  fi
}

# Runs on EXIT, so it ends on a success to preserve the script's status
cleanup() {
  echo "Cleaning up processes..."
  stop_recorder
  kill_stack
  true
}
trap cleanup EXIT INT TERM

# Clear orphaned processes and locks
kill_stack
rm -f /tmp/px4_lock-0 /tmp/px4-sock-0

cd "$REPO"
rm -rf "$BAG_DIR"

RANDOM=$SEED

# Yaw varies the flight. Noise is unseeded, and the world fixes the global origin.
# Drawn here rather than inside the substitution below: bash reseeds RANDOM in a subshell
YAW_DRAW=$RANDOM
HOME_YAW_RAD=$(awk -v r="$YAW_DRAW" 'BEGIN { printf "%.4f", r / 32767.0 * 6.2832 }')
export PX4_GZ_MODEL_POSE="0,0,0,0,0,$HOME_YAW_RAD"

# Optical flow needs ground texture to track, see ADR-0003
export PX4_GZ_WORLD=baylands

echo "Spawn: YAW $HOME_YAW_RAD rad in $PX4_GZ_WORLD"

cd ~/PX4-Autopilot
HEADLESS=1 make px4_sitl "$MODEL" &
PX4_PID=$!
disown "$PX4_PID"

MicroXRCEAgent udp4 -p 8888 &
AGENT_PID=$!

echo "Waiting for PX4 telemetry..."
WAITED_S=0
while ! ros2 topic list 2>/dev/null | grep -q '/fmu/out/vehicle_status_v1'; do
  sleep 1
  WAITED_S=$((WAITED_S + 1))
  if [ "$WAITED_S" -ge "$SIM_TIMEOUT_S" ]; then
    echo "No telemetry after ${SIM_TIMEOUT_S}s"
    exit 2
  fi
done

# Topics appear before EKF2 has a position
echo "Waiting for a position estimate..."
STARTED_S=$SECONDS
until timeout 5 ros2 topic echo --once --qos-reliability best_effort \
  /fmu/out/vehicle_local_position_v1 2>/dev/null | grep -q '^xy_valid: true'; do
  sleep 2
  if [ "$((SECONDS - STARTED_S))" -ge "$SIM_TIMEOUT_S" ]; then
    echo "No position estimate after ${SIM_TIMEOUT_S}s"
    exit 2
  fi
done

# PX4's uXRCE-DDS bridge carries uORB messages, so the camera image needs its own. The gz
# topic is scoped to the world and the spawned model, so ask gz for it rather than guess.
if [ "$MODEL" = gz_x500_flow ]; then
  echo "Waiting for the flow camera topic..."
  WAITED_S=0
  until CAMERA_GZ_TOPIC=$(timeout 5 gz topic -l 2>/dev/null | grep -m1 '/flow_camera/image$'); do
    sleep 1
    WAITED_S=$((WAITED_S + 1))
    if [ "$WAITED_S" -ge 30 ]; then
      echo "No flow camera topic after ${WAITED_S}s"
      exit 2
    fi
  done

  echo "Bridging $CAMERA_GZ_TOPIC to $CAMERA_TOPIC"
  ros2 run ros_gz_bridge parameter_bridge \
    "$CAMERA_GZ_TOPIC@sensor_msgs/msg/Image[gz.msgs.Image" \
    --ros-args -r "$CAMERA_GZ_TOPIC:=$CAMERA_TOPIC" &
  BRIDGE_PID=$!

  # A bridge that connects to nothing still advertises the topic
  STARTED_S=$SECONDS
  until timeout 5 ros2 topic echo --once --qos-reliability best_effort \
    "$CAMERA_TOPIC" >/dev/null 2>&1; do
    sleep 1
    if [ "$((SECONDS - STARTED_S))" -ge 30 ]; then
      echo "No frames on $CAMERA_TOPIC after 30s"
      exit 2
    fi
  done
fi

echo "Starting the relay with scenario $SCENARIO_NAME..."

# The drawn onsets are only in the relay's own output, and the bag folder does not exist yet
RELAY_LOG=$(mktemp)
# An empty scenario_file override is not a parameter rule rcl will parse, so leave it out
RELAY_ARGS=(--ros-args -p seed:="$SEED")
if [ -n "$SCENARIO" ]; then
  RELAY_ARGS+=(-p scenario_file:="$SCENARIO")
fi

ros2 run quorum_injector relay_node "${RELAY_ARGS[@]}" >"$RELAY_LOG" 2>&1 &
RELAY_PID=$!

# Everything downstream reads the twins, so a dead relay is a dead run
WAITED_S=0
while ! ros2 topic list 2>/dev/null | grep -q '/quorum/vehicle_gps_position'; do
  sleep 1
  WAITED_S=$((WAITED_S + 1))
  if [ "$WAITED_S" -ge 20 ]; then
    echo "No relayed topics after ${WAITED_S}s"
    cat "$RELAY_LOG"
    exit 2
  fi
done

# The relay advertises every twin at startup, frames are what it can fail to carry
if [ "$MODEL" = gz_x500_flow ]; then
  STARTED_S=$SECONDS
  until timeout 5 ros2 topic echo --once --qos-reliability best_effort \
    "$CAMERA_TWIN" >/dev/null 2>&1; do
    sleep 1
    if [ "$((SECONDS - STARTED_S))" -ge 30 ]; then
      echo "No frames on $CAMERA_TWIN after 30s"
      exit 2
    fi
  done
fi

echo "Starting the gnss monitor..."

GNSS_MONITOR_LOG=$(mktemp)
ros2 run quorum_monitors gnss_monitor_node >"$GNSS_MONITOR_LOG" 2>&1 &
GNSS_MONITOR_PID=$!

# It only publishes once it holds an anchor, so its own topic is not the thing to wait on
WAITED_S=0
while ! ros2 node list 2>/dev/null | grep -q '/gnss_monitor'; do
  sleep 1
  WAITED_S=$((WAITED_S + 1))
  if [ "$WAITED_S" -ge 20 ]; then
    echo "No gnss monitor after ${WAITED_S}s"
    cat "$GNSS_MONITOR_LOG"
    exit 2
  fi
done

# Flow only exists on this airframe, so the monitor reading it has nothing to do otherwise
FLOW_MONITOR_LOG=$(mktemp)
if [ "$MODEL" = gz_x500_flow ]; then
  echo "Starting the flow monitor..."

  ros2 run quorum_monitors flow_monitor_node >"$FLOW_MONITOR_LOG" 2>&1 &
  FLOW_MONITOR_PID=$!

  WAITED_S=0
  while ! ros2 node list 2>/dev/null | grep -q '/flow_monitor'; do
    sleep 1
    WAITED_S=$((WAITED_S + 1))
    if [ "$WAITED_S" -ge 20 ]; then
      echo "No flow monitor after ${WAITED_S}s"
      cat "$FLOW_MONITOR_LOG"
      exit 2
    fi
  done
fi

cd "$REPO"
TOPICS=(
  /quorum/gnss_residual
  /quorum/trust/gnss
  /fmu/out/vehicle_gps_position
  /fmu/out/sensor_combined
  /fmu/out/vehicle_attitude
  /fmu/out/vehicle_local_position_v1
  /fmu/out/vehicle_status_v1
  /fmu/out/vehicle_land_detected
  /quorum/vehicle_gps_position
  /quorum/sensor_combined
  /quorum/vehicle_attitude
)

# Only the flow airframe carries these
if [ "$MODEL" = gz_x500_flow ]; then
  TOPICS+=(
    /fmu/out/vehicle_optical_flow_vel
    /fmu/out/sensor_optical_flow
    /fmu/out/distance_sensor
    /quorum/vehicle_optical_flow_vel
    /quorum/sensor_optical_flow
    /quorum/distance_sensor
    /quorum/flow_residual
    /quorum/trust/flow
    "$CAMERA_TOPIC"
    "$CAMERA_TWIN"
  )
fi

ros2 bag record -o "$BAG_DIR" "${TOPICS[@]}" &
RECORDER_PID=$!

# Let the bag open first so it captures the takeoff
WAITED_S=0
while [ ! -d "$BAG_DIR" ]; do
  sleep 1
  WAITED_S=$((WAITED_S + 1))
  if [ "$WAITED_S" -ge 20 ]; then
    echo "Recorder never created $BAG_DIR"
    exit 2
  fi
done

echo "Executing flight node..."

# Carry on through a failed flight so the manifest records its code
set +e
timeout "$FLIGHT_TIMEOUT_S" ros2 run quorum_offboard offboard_node \
  --ros-args --params-file "$MISSION_FILE"
EXIT_CODE=$?
set -e

stop_recorder

PX4_VERSION=$(git -C ~/PX4-Autopilot describe --tags 2>/dev/null || echo unknown)

cat > "$BAG_DIR/manifest.txt" <<EOF
run: $RUN
mission: $MISSION
model: $MODEL
world: $PX4_GZ_WORLD
seed: $SEED
date: $(date --iso-8601=seconds)
commit: $(git rev-parse --short HEAD)
px4_version: $PX4_VERSION
home_yaw_rad: $HOME_YAW_RAD
camera_gz_topic: ${CAMERA_GZ_TOPIC:-none}
scenario: $SCENARIO_NAME
exit_code: $EXIT_CODE
EOF

mv "$RELAY_LOG" "$BAG_DIR/relay.log"
mv "$GNSS_MONITOR_LOG" "$BAG_DIR/gnss_monitor.log"

if [ "$MODEL" = gz_x500_flow ]; then
  mv "$FLOW_MONITOR_LOG" "$BAG_DIR/flow_monitor.log"
fi

echo "Finished $RUN with exit code $EXIT_CODE"
sed 's/.*\] //' "$BAG_DIR/relay.log"
exit "$EXIT_CODE"
