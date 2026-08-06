#ifndef QUORUM_OFFBOARD__MISSION_CONFIG_HPP_
#define QUORUM_OFFBOARD__MISSION_CONFIG_HPP_

#include <cassert>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "quorum_offboard/mission.hpp"

namespace quorum_offboard {

/// A waypoint as it arrives from the parameter server, before any checking
struct RawWaypoint {
  std::string name;
  std::vector<double> position_ned_m;
};

/// A mission as it arrives from the parameter server, before any checking
struct RawMission {
  std::string frame;
  double takeoff_altitude_m;
  double cruise_speed_mps;
  double acceptance_radius_m;
  double hold_time_s;
  std::vector<RawWaypoint> waypoints;
};

/// A mission that has passed validation
struct MissionSpec {
  double takeoff_altitude_m;
  double cruise_speed_mps;
  double acceptance_radius_m;
  double hold_time_s;
  std::vector<PositionNed> waypoints;
};

/// kBadAltitude covers a take-off altitude at or below the ground and a waypoint below it
enum class MissionError {
  kUnknownFrame,
  kNoWaypoints,
  kBadPositionLength,
  kBadAltitude,
  kBadRadius,
  kBadSpeed,
  kBadHoldTime
};

[[nodiscard]] inline std::string_view to_string(MissionError error)
{
  switch (error) {
    case MissionError::kUnknownFrame:
      return "Unknown Frame";

    case MissionError::kNoWaypoints:
      return "No Waypoints";

    case MissionError::kBadPositionLength:
      return "Bad Position Length";

    case MissionError::kBadAltitude:
      return "Bad Altitude";

    case MissionError::kBadRadius:
      return "Bad Radius";

    case MissionError::kBadSpeed:
      return "Bad Speed";

    case MissionError::kBadHoldTime:
      return "Bad Hold Time";
  }
  return "Unknown Mission Error";
}

/// Down is positive downwards, so a positive third component sits below the take-off surface
[[nodiscard]] inline std::expected<PositionNed, MissionError> validate_raw_waypoint(
    const RawWaypoint& raw_waypoint)
{
  const std::vector<double>& position_ned_m = raw_waypoint.position_ned_m;

  if (position_ned_m.size() != 3) {
    return std::unexpected(MissionError::kBadPositionLength);
  }

  if (position_ned_m[2] > 0.0) {
    return std::unexpected(MissionError::kBadAltitude);
  }

  return PositionNed{position_ned_m[0], position_ned_m[1], position_ned_m[2]};
}

/// Reports the first problem it finds
[[nodiscard]] inline std::expected<MissionSpec, MissionError> make_mission_spec(
    const RawMission& raw)
{
  if (raw.frame != "ned") {
    return std::unexpected(MissionError::kUnknownFrame);
  }

  if (raw.waypoints.empty()) {
    return std::unexpected(MissionError::kNoWaypoints);
  }

  if (raw.takeoff_altitude_m <= 0.0) {
    return std::unexpected(MissionError::kBadAltitude);
  }

  if (raw.acceptance_radius_m <= 0.0) {
    return std::unexpected(MissionError::kBadRadius);
  }

  if (raw.cruise_speed_mps <= 0.0) {
    return std::unexpected(MissionError::kBadSpeed);
  }

  // Zero is valid: fly straight through.
  if (raw.hold_time_s < 0.0) {
    return std::unexpected(MissionError::kBadHoldTime);
  }

  std::vector<PositionNed> waypoints;
  waypoints.reserve(raw.waypoints.size());

  for (const RawWaypoint& raw_waypoint : raw.waypoints) {
    const std::expected<PositionNed, MissionError> result = validate_raw_waypoint(raw_waypoint);
    if (!result.has_value()) {
      return std::unexpected(result.error());
    }
    waypoints.push_back(result.value());
  }

  assert(waypoints.size() == raw.waypoints.size());

  return MissionSpec{raw.takeoff_altitude_m, raw.cruise_speed_mps, raw.acceptance_radius_m,
                     raw.hold_time_s, std::move(waypoints)};
}

}  // namespace quorum_offboard

#endif  // QUORUM_OFFBOARD__MISSION_CONFIG_HPP_
