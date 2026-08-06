#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "quorum_offboard/mission_config.hpp"

namespace quorum_offboard {
namespace {

constexpr double kAltitudeM = 5.0;
constexpr double kSpeedMps = 3.0;
constexpr double kRadiusM = 1.5;
constexpr double kHoldS = 2.0;

std::vector<RawWaypoint> square()
{
  return {{"ne", {20.0, 0.0, -5.0}}, {"nw", {20.0, 20.0, -5.0}}, {"home", {0.0, 0.0, -5.0}}};
}

RawMission square_mission()
{
  return RawMission{"ned", kAltitudeM, kSpeedMps, kRadiusM, kHoldS, square()};
}

std::expected<MissionSpec, MissionError> make_square()
{
  return make_mission_spec(square_mission());
}

TEST(MakeMissionSpec, KeepsTheWaypointsInFileOrder)
{
  const auto spec = make_square();
  ASSERT_TRUE(spec.has_value());

  ASSERT_EQ(spec->waypoints.size(), 3u);
  EXPECT_DOUBLE_EQ(spec->waypoints[0].east_m, 0.0);
  EXPECT_DOUBLE_EQ(spec->waypoints[1].east_m, 20.0);
  EXPECT_DOUBLE_EQ(spec->waypoints[2].north_m, 0.0);
}

TEST(MakeMissionSpec, CopiesTheScalarsThrough)
{
  const auto spec = make_square();
  ASSERT_TRUE(spec.has_value());

  EXPECT_DOUBLE_EQ(spec->takeoff_altitude_m, kAltitudeM);
  EXPECT_DOUBLE_EQ(spec->cruise_speed_mps, kSpeedMps);
  EXPECT_DOUBLE_EQ(spec->acceptance_radius_m, kRadiusM);
  EXPECT_DOUBLE_EQ(spec->hold_time_s, kHoldS);
}

TEST(MakeMissionSpec, ReadsPositionsAsNorthEastDown)
{
  RawMission raw = square_mission();
  raw.waypoints = {{"one", {1.0, 2.0, -3.0}}};

  const auto spec = make_mission_spec(raw);
  ASSERT_TRUE(spec.has_value());

  EXPECT_DOUBLE_EQ(spec->waypoints[0].north_m, 1.0);
  EXPECT_DOUBLE_EQ(spec->waypoints[0].east_m, 2.0);
  EXPECT_DOUBLE_EQ(spec->waypoints[0].down_m, -3.0);
}

TEST(MakeMissionSpec, RejectsAFrameItCannotConvert)
{
  RawMission raw = square_mission();
  raw.frame = "enu";

  const auto spec = make_mission_spec(raw);

  ASSERT_FALSE(spec.has_value());
  EXPECT_EQ(spec.error(), MissionError::kUnknownFrame);
}

TEST(MakeMissionSpec, RejectsAnEmptyWaypointList)
{
  RawMission raw = square_mission();
  raw.waypoints.clear();

  const auto spec = make_mission_spec(raw);

  ASSERT_FALSE(spec.has_value());
  EXPECT_EQ(spec.error(), MissionError::kNoWaypoints);
}

// Three of the scalars must be strictly positive; zero is the interesting edge because a mission
// file that omits a key gets one. Hold time is the exception and is tested separately.
struct ScalarCase {
  const char* name;
  double takeoff_altitude_m;
  double cruise_speed_mps;
  double acceptance_radius_m;
  double hold_time_s;
  MissionError expected;
};

class BadScalar : public testing::TestWithParam<ScalarCase> {};

TEST_P(BadScalar, IsRejected)
{
  const ScalarCase& scalar_case = GetParam();
  RawMission raw = square_mission();
  raw.takeoff_altitude_m = scalar_case.takeoff_altitude_m;
  raw.cruise_speed_mps = scalar_case.cruise_speed_mps;
  raw.acceptance_radius_m = scalar_case.acceptance_radius_m;
  raw.hold_time_s = scalar_case.hold_time_s;

  const auto spec = make_mission_spec(raw);

  ASSERT_FALSE(spec.has_value());
  EXPECT_EQ(spec.error(), scalar_case.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Scalars, BadScalar,
    testing::Values(
        ScalarCase{"zero_altitude", 0.0, kSpeedMps, kRadiusM, kHoldS, MissionError::kBadAltitude},
        ScalarCase{"negative_altitude", -5.0, kSpeedMps, kRadiusM, kHoldS,
                   MissionError::kBadAltitude},
        ScalarCase{"zero_speed", kAltitudeM, 0.0, kRadiusM, kHoldS, MissionError::kBadSpeed},
        ScalarCase{"negative_speed", kAltitudeM, -3.0, kRadiusM, kHoldS, MissionError::kBadSpeed},
        ScalarCase{"zero_radius", kAltitudeM, kSpeedMps, 0.0, kHoldS, MissionError::kBadRadius},
        ScalarCase{"negative_radius", kAltitudeM, kSpeedMps, -1.5, kHoldS,
                   MissionError::kBadRadius},
        ScalarCase{"negative_hold", kAltitudeM, kSpeedMps, kRadiusM, -1.0,
                   MissionError::kBadHoldTime}),
    [](const testing::TestParamInfo<ScalarCase>& param) { return param.param.name; });

TEST(MakeMissionSpec, AcceptsAZeroHoldTime)
{
  RawMission raw = square_mission();
  raw.hold_time_s = 0.0;

  const auto spec = make_mission_spec(raw);

  ASSERT_TRUE(spec.has_value());
  EXPECT_DOUBLE_EQ(spec->hold_time_s, 0.0);
}

TEST(MakeMissionSpec, ReportsTheFirstBadWaypoint)
{
  RawMission raw = square_mission();
  raw.waypoints = {
      {"good", {0.0, 0.0, -5.0}}, {"short", {1.0, 2.0}}, {"underground", {3.0, 4.0, 5.0}}};

  const auto spec = make_mission_spec(raw);

  ASSERT_FALSE(spec.has_value());
  EXPECT_EQ(spec.error(), MissionError::kBadPositionLength);
}

TEST(ValidateRawWaypoint, NeedsExactlyThreeComponents)
{
  EXPECT_EQ(validate_raw_waypoint({"two", {1.0, 2.0}}).error(), MissionError::kBadPositionLength);
  EXPECT_EQ(validate_raw_waypoint({"four", {1.0, 2.0, -3.0, 4.0}}).error(),
            MissionError::kBadPositionLength);
  EXPECT_TRUE(validate_raw_waypoint({"three", {1.0, 2.0, -3.0}}).has_value());
}

// Down is positive downwards, so a positive z is a waypoint below the take-off surface.
TEST(ValidateRawWaypoint, RejectsAPositiveDownComponent)
{
  EXPECT_EQ(validate_raw_waypoint({"underground", {0.0, 0.0, 0.1}}).error(),
            MissionError::kBadAltitude);
  EXPECT_TRUE(validate_raw_waypoint({"ground", {0.0, 0.0, 0.0}}).has_value());
}

TEST(ToString, NamesEveryError)
{
  const std::vector<MissionError> errors{
      MissionError::kUnknownFrame, MissionError::kNoWaypoints, MissionError::kBadPositionLength,
      MissionError::kBadAltitude,  MissionError::kBadRadius,   MissionError::kBadSpeed,
      MissionError::kBadHoldTime};

  for (MissionError error : errors) {
    EXPECT_NE(std::string(to_string(error)), "Unknown Mission Error");
  }
}

}  // namespace
}  // namespace quorum_offboard
