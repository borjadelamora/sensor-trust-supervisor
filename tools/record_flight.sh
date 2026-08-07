#!/usr/bin/env bash
set -e

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
  echo "Usage: $0 <mission_name> <seed> [px4_model]"
  exit 1
fi

MISSION=$1
SEED=$2
MODEL=${3:-gz_x500_flow}

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
MISSION_FILE="$REPO/src/quorum_offboard/config/missions/${MISSION}.yaml"
RUN="$(date +%Y%m%d)_${MISSION}_s${SEED}"
BAG_DIR="$REPO/data/$RUN"

# Baylands is a large world and can take a couple of minutes to load
SIM_TIMEOUT_S=300
FLIGHT_TIMEOUT_S=300

# Exit status: 1 bad arguments, 2 stack never came up, 124 flight overran, else the node's.
if [ ! -f "$MISSION_FILE" ]; then
  echo "No mission file at $MISSION_FILE"
  exit 1
fi

echo "Run $RUN"

# Background processes inherit the ROS environment
source ~/px4_ros_ws/install/setup.bash
source "$REPO/install/setup.bash"

RECORDER_PID=""
PX4_PID=""
AGENT_PID=""

kill_stack() {
  pkill -9 -f ninja || true
  kill -9 "$PX4_PID" 2>/dev/null || true
  kill -9 "$AGENT_PID" 2>/dev/null || true
  pkill -9 -f MicroXRCEAgent || true
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
HOME_YAW_RAD=$(awk -v r="$RANDOM" 'BEGIN { printf "%.4f", r / 32767.0 * 6.2832 }')
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

cd "$REPO"
TOPICS=(
  /fmu/out/vehicle_gps_position
  /fmu/out/sensor_combined
  /fmu/out/vehicle_attitude
  /fmu/out/vehicle_local_position_v1
  /fmu/out/vehicle_status_v1
  /fmu/out/vehicle_land_detected
)

# Only the flow airframe carries these
if [ "$MODEL" = gz_x500_flow ]; then
  TOPICS+=(
    /fmu/out/vehicle_optical_flow_vel
    /fmu/out/sensor_optical_flow
    /fmu/out/distance_sensor
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
exit_code: $EXIT_CODE
EOF

echo "Finished $RUN with exit code $EXIT_CODE"
exit "$EXIT_CODE"
