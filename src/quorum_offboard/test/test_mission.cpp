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

TEST(StepTowards, CoversTheStepDistanceAlongTheLine)
{
  const PositionNed stepped = step_towards({0.0, 0.0, -5.0}, {30.0, 40.0, -5.0}, 5.0);

  EXPECT_DOUBLE_EQ(stepped.north_m, 3.0);
  EXPECT_DOUBLE_EQ(stepped.east_m, 4.0);
  EXPECT_DOUBLE_EQ(stepped.down_m, -5.0);
}

TEST(StepTowards, StopsAtTheTargetRatherThanOvershooting)
{
  const PositionNed target{2.0, 0.0, -5.0};
  const PositionNed stepped = step_towards({0.0, 0.0, -5.0}, target, 10.0);

  EXPECT_DOUBLE_EQ(stepped.north_m, target.north_m);
  EXPECT_DOUBLE_EQ(stepped.east_m, target.east_m);
}

TEST(StepTowards, ClimbsAsWellAsTranslates)
{
  const PositionNed stepped = step_towards({0.0, 0.0, 0.0}, {0.0, 0.0, -10.0}, 2.0);

  EXPECT_DOUBLE_EQ(stepped.down_m, -2.0);
}

TEST(StepTowards, HoldsPositionOnAZeroStep)
{
  const PositionNed stepped = step_towards({1.0, 2.0, -5.0}, {30.0, 40.0, -5.0}, 0.0);

  EXPECT_DOUBLE_EQ(stepped.north_m, 1.0);
  EXPECT_DOUBLE_EQ(stepped.east_m, 2.0);
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
