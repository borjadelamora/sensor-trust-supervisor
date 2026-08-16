#include <gtest/gtest.h>

#include "quorum_monitors/flow_monitor.hpp"

namespace quorum_monitors {
namespace {

constexpr ReferenceLimits kReferenceLimits{
    .max_gap_s = 0.05, .max_age_s = 10.0, .max_attitude_age_s = 0.1};

constexpr DebounceLimits kDebounceLimits{.threshold_mps = 1.0, .sustain_s = 2.0};

constexpr Attitude kLevel{.w = 1.0};

constexpr AccelerationFrd kLevelAtRest{.down_mps2 = -kGravityMps2};

constexpr double kStartS = 20.0;

// One interval of the 100 Hz imu, the fastest of the three streams
constexpr double kStepS = 0.01;

constexpr uint8_t kFirstReset = 0;

// Feeds all three streams at one stamp and hands back what the flow sample produced
std::expected<double, FlowError> step(FlowMonitor& monitor, const VelocityNe& velocity,
                                      double time_s)
{
  monitor.on_attitude(AttitudeSample{.attitude = kLevel, .time_s = time_s}, kFirstReset);
  static_cast<void>(monitor.on_imu(ImuSample{.acceleration = kLevelAtRest, .time_s = time_s}));

  return monitor.on_flow(FlowSample{.velocity = velocity, .time_s = time_s});
}

TEST(FlowMonitor, AnchorsOnTheFirstFlowSampleAndComparesAgainstTheNext)
{
  FlowMonitor monitor(kReferenceLimits, kDebounceLimits);

  EXPECT_EQ(step(monitor, VelocityNe{}, kStartS).error(), FlowError::kNoReference);
  EXPECT_TRUE(step(monitor, VelocityNe{}, kStartS + kStepS).has_value());
  EXPECT_EQ(step(monitor, VelocityNe{}, kStartS + kStepS).error(), FlowError::kNotAdvanced);
}

// An ekf2 reset invalidates what was integrated, so the next sample has to anchor again
TEST(FlowMonitor, DropsTheReferenceWhenTheAttitudeResets)
{
  const double reset_at_s = kStartS + (2.0 * kStepS);

  FlowMonitor monitor(kReferenceLimits, kDebounceLimits);

  static_cast<void>(step(monitor, VelocityNe{}, kStartS));
  static_cast<void>(step(monitor, VelocityNe{}, kStartS + kStepS));

  monitor.on_attitude(AttitudeSample{.attitude = kLevel, .time_s = reset_at_s}, kFirstReset + 1);

  EXPECT_EQ(monitor.on_flow(FlowSample{.time_s = reset_at_s}).error(), FlowError::kNoReference);
}

TEST(FlowMonitor, TurnsSuspectAfterFlowDisagreesForTheSustainPeriod)
{
  constexpr VelocityNe kDisagreeing{.north_mps = 2.0};
  constexpr int kSustainedSteps = static_cast<int>(kDebounceLimits.sustain_s / kStepS);

  FlowMonitor monitor(kReferenceLimits, kDebounceLimits);
  static_cast<void>(step(monitor, VelocityNe{}, kStartS));

  for (int taken = 1; taken <= kSustainedSteps; ++taken) {
    static_cast<void>(step(monitor, kDisagreeing, kStartS + (kStepS * static_cast<double>(taken))));
  }

  EXPECT_EQ(monitor.level(), TrustLevel::kTrusted);
  EXPECT_NEAR(monitor.judgement()->residual_mps, kDisagreeing.north_mps, 1e-9);

  static_cast<void>(
      step(monitor, kDisagreeing, kStartS + (kStepS * static_cast<double>(kSustainedSteps + 1))));

  EXPECT_EQ(monitor.level(), TrustLevel::kSuspect);
}

}  // namespace
}  // namespace quorum_monitors
