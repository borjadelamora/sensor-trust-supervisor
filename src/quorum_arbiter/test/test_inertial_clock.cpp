#include <gtest/gtest.h>

#include "quorum_arbiter/inertial_clock.hpp"

namespace quorum_arbiter {
namespace {

// One interval of the 100 Hz imu, and the reference's gap limit as the cap
constexpr double kStepS = 0.01;
constexpr double kMaxStepS = 0.05;

TEST(InertialClock, CountsASteadyStream)
{
  InertialClock clock(kMaxStepS);
  for (int i = 0; i <= 100; ++i) {
    clock.advance(100.0 + i * kStepS);
  }

  EXPECT_NEAR(clock.now_s(), 1.0, 1e-9);
}

TEST(InertialClock, CountsAJumpAsTheCap)
{
  InertialClock clock(kMaxStepS);
  clock.advance(100.0);
  clock.advance(103.0);

  EXPECT_DOUBLE_EQ(clock.now_s(), kMaxStepS);
}

TEST(InertialClock, NeitherRewindsNorStallsOnABackwardsStamp)
{
  InertialClock clock(kMaxStepS);
  clock.advance(100.0);
  clock.advance(99.0);
  clock.advance(99.0 + kStepS);

  EXPECT_NEAR(clock.now_s(), kStepS, 1e-9);
}

}  // namespace
}  // namespace quorum_arbiter
