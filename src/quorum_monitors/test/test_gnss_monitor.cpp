#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "quorum_common/velocity_reference.hpp"

#include "quorum_monitors/gnss_monitor.hpp"

namespace quorum_monitors {

using quorum_common::AccelerationFrd;
using quorum_common::Attitude;
using quorum_common::AttitudeSample;
using quorum_common::ImuSample;
using quorum_common::kGravityMps2;
using quorum_common::ReferenceLimits;
using quorum_common::VelocityNed;

namespace {

// Baylands' origin, the world every recorded flight starts from
constexpr GeoPosition kBase{37.412173071650805, -121.998878727967};

constexpr GateLimits kGateLimits{.limit_mps = 12.0, .min_interval_s = 1.0, .max_interval_s = 2.0};

constexpr ReferenceLimits kReferenceLimits{
    .max_gap_s = 0.05, .max_age_s = 10.0, .max_attitude_age_s = 0.1};

// Above the 1.40 m/s benign ceiling EXP-20260815-001 measured
constexpr DebounceLimits kDebounceLimits{.threshold = 2.0, .sustain_s = 2.0};

constexpr Attitude kLevel{.w = 1.0};

constexpr AccelerationFrd kLevelAtRest{.down_mps2 = -kGravityMps2};

constexpr double kStartS = 20.0;

// One interval of the 100 Hz imu
constexpr double kStepS = 0.01;

constexpr uint8_t kFirstReset = 0;

GeoPosition shifted(double north_m, double east_m)
{
  const double east_scale = std::cos(to_radians(kBase.latitude_deg));
  return {
      kBase.latitude_deg + ((north_m / kEarthRadiusM) * 180.0 / std::numbers::pi),
      kBase.longitude_deg + ((east_m / (kEarthRadiusM * east_scale)) * 180.0 / std::numbers::pi)};
}

// Feeds the attitude and imu streams at one stamp and hands back what the fix produced
std::expected<VelocityNed, ResidualError> step(GnssMonitor& monitor, const VelocityNed& gps,
                                               double time_s)
{
  monitor.on_attitude(AttitudeSample{.attitude = kLevel, .time_s = time_s}, kFirstReset);
  static_cast<void>(monitor.on_imu(ImuSample{.acceleration = kLevelAtRest, .time_s = time_s}));

  return monitor.check_velocity(gps, time_s);
}

TEST(DisplacementM, ConvertsADegreeDeltaToMetresOnBothAxes)
{
  EXPECT_NEAR(displacement_m(kBase, {kBase.latitude_deg + 0.001, kBase.longitude_deg}).north_m,
              111.1949, 1e-3);
  EXPECT_NEAR(displacement_m(kBase, {kBase.latitude_deg, kBase.longitude_deg + 0.001}).east_m,
              88.3205, 1e-3);
}

// The long interval is a 50 m step spread across one of PX4's catch-up bursts
TEST(CheckKinematicGate, JudgesOnlyIntervalsInsideTheBounds)
{
  const FixSample first{kBase, 20.0};

  EXPECT_EQ(check_kinematic_gate(first, {shifted(0.1, 0.0), 20.033}, kGateLimits).error(),
            GateError::kTooSoon);
  EXPECT_EQ(check_kinematic_gate(first, {shifted(25.0, 43.3), 26.0}, kGateLimits).error(),
            GateError::kGap);
}

TEST(CheckKinematicGate, SeparatesCruiseSpeedFromAStep)
{
  const FixSample first{kBase, 20.0};
  const std::expected<GateVerdict, GateError> cruising =
      check_kinematic_gate(first, {shifted(3.0, 0.0), 21.0}, kGateLimits);
  const std::expected<GateVerdict, GateError> jumped =
      check_kinematic_gate(first, {shifted(25.0, 43.30127), 21.0}, kGateLimits);

  ASSERT_TRUE(cruising.has_value());
  ASSERT_TRUE(jumped.has_value());
  EXPECT_NEAR(cruising->speed_mps, 3.0, 1e-6);
  EXPECT_FALSE(cruising->exceeded);
  EXPECT_NEAR(jumped->speed_mps, 50.0, 1e-3);
  EXPECT_TRUE(jumped->exceeded);
}

TEST(GnssMonitor, HasNothingToCompareTheFirstFixAgainst)
{
  GnssMonitor monitor(kGateLimits, kReferenceLimits, kDebounceLimits);

  EXPECT_EQ(monitor.check_gate(FixSample{kBase, kStartS}).error(), GateError::kNoPreviousFix);
  EXPECT_TRUE(monitor.check_gate(FixSample{shifted(3.0, 0.0), kStartS + 1.0}).has_value());
}

TEST(GnssMonitor, AnchorsOnTheFirstFixAndComparesAgainstTheNext)
{
  GnssMonitor monitor(kGateLimits, kReferenceLimits, kDebounceLimits);

  EXPECT_EQ(step(monitor, VelocityNed{}, kStartS).error(), ResidualError::kNoReference);
  EXPECT_TRUE(step(monitor, VelocityNed{}, kStartS + kStepS).has_value());
  EXPECT_EQ(step(monitor, VelocityNed{}, kStartS + kStepS).error(), ResidualError::kNotAdvanced);
}

TEST(GnssMonitor, TurnsSuspectAfterTheFixDisagreesForTheSustainPeriod)
{
  constexpr VelocityNed kDisagreeing{.north_mps = 3.0};
  constexpr int kSustainedSteps = static_cast<int>(kDebounceLimits.sustain_s / kStepS);

  GnssMonitor monitor(kGateLimits, kReferenceLimits, kDebounceLimits);
  static_cast<void>(step(monitor, VelocityNed{}, kStartS));

  for (int taken = 1; taken <= kSustainedSteps; ++taken) {
    static_cast<void>(step(monitor, kDisagreeing, kStartS + (kStepS * static_cast<double>(taken))));
  }

  EXPECT_EQ(monitor.level(), TrustLevel::kTrusted);
  EXPECT_NEAR(monitor.judgement()->residual, kDisagreeing.north_mps, 1e-9);

  static_cast<void>(
      step(monitor, kDisagreeing, kStartS + (kStepS * static_cast<double>(kSustainedSteps + 1))));

  EXPECT_EQ(monitor.level(), TrustLevel::kSuspect);
}

}  // namespace
}  // namespace quorum_monitors
