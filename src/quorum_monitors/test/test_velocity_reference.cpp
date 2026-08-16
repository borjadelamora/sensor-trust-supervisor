#include <gtest/gtest.h>

#include "quorum_monitors/velocity_reference.hpp"

namespace quorum_monitors {
namespace {

constexpr double kHalfRootTwo = 0.707106781186547524;

constexpr Attitude kLevel{.w = 1.0};

// A quarter turn about the down axis, nose to the east
constexpr Attitude kNoseEast{.w = kHalfRootTwo, .z = kHalfRootTwo};

// A quarter turn about the right axis, nose to the sky
constexpr Attitude kNoseUp{.w = kHalfRootTwo, .y = kHalfRootTwo};

// What a level accelerometer reads standing still
constexpr AccelerationFrd kLevelAtRest{.down_mps2 = -kGravityMps2};

constexpr ReferenceLimits kLimits{
    .max_gap_s = 0.05, .max_age_s = 10.0, .max_attitude_age_s = 0.1};

constexpr double kAnchoredAtS = 20.0;

TEST(RotateToNed, SendsForwardEastWhenTheNoseIsEast)
{
  const AccelerationNed rotated = rotate_to_ned(AccelerationFrd{.forward_mps2 = 1.0}, kNoseEast);

  EXPECT_NEAR(rotated.north_mps2, 0.0, 1e-12);
  EXPECT_NEAR(rotated.east_mps2, 1.0, 1e-12);
}

// Standing on its tail the accelerometer reads gravity forward, and it still cancels
TEST(RemoveGravity, ZeroesAStillAirframeAtAnyAttitude)
{
  const AccelerationNed level = remove_gravity(rotate_to_ned(kLevelAtRest, kLevel));
  const AccelerationNed pitched =
      remove_gravity(rotate_to_ned(AccelerationFrd{.forward_mps2 = kGravityMps2}, kNoseUp));

  EXPECT_NEAR(level.down_mps2, 0.0, 1e-12);
  EXPECT_NEAR(pitched.north_mps2, 0.0, 1e-12);
  EXPECT_NEAR(pitched.down_mps2, 0.0, 1e-12);
}

TEST(VelocityReference, StaysAtTheAnchorThroughALevelHover)
{
  VelocityReference reference(kLimits);
  reference.anchor(VelocityNed{.north_mps = 3.0}, kAnchoredAtS);

  for (int step = 1; step <= 100; ++step) {
    const double time_s = kAnchoredAtS + (0.01 * static_cast<double>(step));
    ASSERT_TRUE(reference
                    .advance(ImuSample{.acceleration = kLevelAtRest, .time_s = time_s},
                             AttitudeSample{.attitude = kLevel, .time_s = time_s})
                    .has_value());
  }

  const std::expected<VelocityNed, ReferenceError> held = reference.velocity(kAnchoredAtS + 1.0);

  ASSERT_TRUE(held.has_value());
  EXPECT_NEAR(held->north_mps, 3.0, 1e-9);
  EXPECT_NEAR(held->down_mps, 0.0, 1e-9);
}

TEST(VelocityReference, GrowsAtTheAccelerationItIsFed)
{
  constexpr AccelerationFrd kAccelerating{.forward_mps2 = 2.0, .down_mps2 = -kGravityMps2};

  VelocityReference reference(kLimits);
  reference.anchor(VelocityNed{}, kAnchoredAtS);

  for (int step = 1; step <= 50; ++step) {
    const double time_s = kAnchoredAtS + (0.01 * static_cast<double>(step));
    ASSERT_TRUE(reference
                    .advance(ImuSample{.acceleration = kAccelerating, .time_s = time_s},
                             AttitudeSample{.attitude = kLevel, .time_s = time_s})
                    .has_value());
  }

  const std::expected<VelocityNed, ReferenceError> held = reference.velocity(kAnchoredAtS + 0.5);

  ASSERT_TRUE(held.has_value());
  EXPECT_NEAR(held->north_mps, 1.0, 1e-9);
}

// A px4 catch-up burst leaves an interval nothing can be integrated across
TEST(VelocityReference, DropsTheEstimateAcrossAGap)
{
  const double after_the_gap_s = kAnchoredAtS + 1.0;

  VelocityReference reference(kLimits);
  reference.anchor(VelocityNed{}, kAnchoredAtS);

  EXPECT_EQ(reference
                .advance(ImuSample{.acceleration = kLevelAtRest, .time_s = after_the_gap_s},
                         AttitudeSample{.attitude = kLevel, .time_s = after_the_gap_s})
                .error(),
            ReferenceError::kGap);
  EXPECT_FALSE(reference.anchored());
}

TEST(VelocityReference, RefusesAnImuSampleAnOldAttitudeCannotRotate)
{
  VelocityReference reference(kLimits);
  reference.anchor(VelocityNed{}, kAnchoredAtS);

  const ImuSample imu{.acceleration = kLevelAtRest, .time_s = kAnchoredAtS + 0.01};
  const AttitudeSample stale{.attitude = kLevel,
                             .time_s = kAnchoredAtS - kLimits.max_attitude_age_s};

  EXPECT_EQ(reference.advance(imu, stale).error(), ReferenceError::kStaleAttitude);
  EXPECT_TRUE(reference.anchored());
}

TEST(VelocityReference, ExpiresAnAnchorPastItsAge)
{
  VelocityReference reference(kLimits);
  reference.anchor(VelocityNed{}, kAnchoredAtS);

  EXPECT_TRUE(reference.velocity(kAnchoredAtS + kLimits.max_age_s).has_value());
  EXPECT_EQ(reference.velocity(kAnchoredAtS + kLimits.max_age_s + 0.01).error(),
            ReferenceError::kExpired);
}

}  // namespace
}  // namespace quorum_monitors
