#include <gtest/gtest.h>

#include <cmath>

#include "quorum_monitors/gnss_monitor.hpp"

namespace quorum_monitors {
namespace {

// Baylands' origin, the world every recorded flight starts from
constexpr GeoPosition kBase{37.412173071650805, -121.998878727967};

constexpr GateLimits kLimits{.limit_mps = 12.0, .min_interval_s = 1.0, .max_interval_s = 2.0};

GeoPosition shifted(double north_m, double east_m)
{
  const double east_scale = std::cos(to_radians(kBase.latitude_deg));
  return {
      kBase.latitude_deg + ((north_m / kEarthRadiusM) * 180.0 / std::numbers::pi),
      kBase.longitude_deg + ((east_m / (kEarthRadiusM * east_scale)) * 180.0 / std::numbers::pi)};
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

  EXPECT_EQ(check_kinematic_gate(first, {shifted(0.1, 0.0), 20.033}, kLimits).error(),
            GateError::kTooSoon);
  EXPECT_EQ(check_kinematic_gate(first, {shifted(25.0, 43.3), 26.0}, kLimits).error(),
            GateError::kGap);
}

TEST(CheckKinematicGate, SeparatesCruiseSpeedFromAStep)
{
  const FixSample first{kBase, 20.0};
  const std::expected<GateVerdict, GateError> cruising =
      check_kinematic_gate(first, {shifted(3.0, 0.0), 21.0}, kLimits);
  const std::expected<GateVerdict, GateError> jumped =
      check_kinematic_gate(first, {shifted(25.0, 43.30127), 21.0}, kLimits);

  ASSERT_TRUE(cruising.has_value());
  ASSERT_TRUE(jumped.has_value());
  EXPECT_NEAR(cruising->speed_mps, 3.0, 1e-6);
  EXPECT_FALSE(cruising->exceeded);
  EXPECT_NEAR(jumped->speed_mps, 50.0, 1e-3);
  EXPECT_TRUE(jumped->exceeded);
}

}  // namespace
}  // namespace quorum_monitors
