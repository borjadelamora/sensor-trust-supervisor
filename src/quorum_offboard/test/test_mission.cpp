#include <gtest/gtest.h>

#include "quorum_offboard/mission.hpp"

namespace quorum_offboard {
namespace {

constexpr double kRadiusM = 1.5;
constexpr double kToleranceM = 0.5;

TEST(IsWithin, TreatsTheRadiusAsTheBoundary)
{
  const PositionNed target{10.0, 0.0, -5.0};

  EXPECT_TRUE(is_within({11.5, 0.0, -5.0}, target, kRadiusM));
  EXPECT_FALSE(is_within({11.6, 0.0, -5.0}, target, kRadiusM));
}

TEST(IsWithin, MeasuresBothHorizontalAxes)
{
  const PositionNed target{0.0, 0.0, -5.0};

  EXPECT_TRUE(is_within({0.6, 0.8, -5.0}, target, kRadiusM));
  EXPECT_FALSE(is_within({1.2, 1.6, -5.0}, target, kRadiusM));
}

TEST(IsWithin, IgnoresAltitude)
{
  const PositionNed target{10.0, 0.0, -5.0};

  EXPECT_TRUE(is_within({10.0, 0.0, -40.0}, target, kRadiusM));
}

TEST(IsAtAltitude, AcceptsEitherSideOfTheTolerance)
{
  EXPECT_TRUE(is_at_altitude({0.0, 0.0, -4.7}, -5.0, kToleranceM));
  EXPECT_TRUE(is_at_altitude({0.0, 0.0, -5.3}, -5.0, kToleranceM));
  EXPECT_FALSE(is_at_altitude({0.0, 0.0, -4.2}, -5.0, kToleranceM));
}

TEST(Mission, WalksTheWaypointsInOrderThenFinishes)
{
  Mission mission{{{1.0, 2.0, -5.0}, {3.0, 4.0, -5.0}}};

  EXPECT_DOUBLE_EQ(mission.current().north_m, 1.0);

  mission.advance();
  EXPECT_DOUBLE_EQ(mission.current().north_m, 3.0);

  mission.advance();
  EXPECT_TRUE(mission.finished());
}

}  // namespace
}  // namespace quorum_offboard
