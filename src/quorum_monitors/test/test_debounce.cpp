#include <gtest/gtest.h>

#include "quorum_monitors/debounce.hpp"

namespace quorum_monitors {
namespace {

constexpr DebounceLimits kLimits{.threshold = 1.0, .sustain_s = 2.0};

constexpr double kOverMps = 1.5;
constexpr double kUnderMps = 0.5;

// One sample of the 45 Hz flow stream
constexpr double kStepS = 1.0 / 45.0;

constexpr double kStartS = 20.0;

TEST(Debounce, StaysTrustedUntilTheExcursionReachesTheSustainPeriod)
{
  Debounce debounce(kLimits);

  debounce.update(kOverMps, kStartS);
  debounce.update(kOverMps, kStartS + kLimits.sustain_s - kStepS);
  EXPECT_EQ(debounce.level(), TrustLevel::kTrusted);

  debounce.update(kOverMps, kStartS + kLimits.sustain_s);
  EXPECT_EQ(debounce.level(), TrustLevel::kSuspect);
}

TEST(Debounce, RestartsTheCountAfterOneSampleBackUnder)
{
  Debounce debounce(kLimits);

  debounce.update(kOverMps, kStartS);
  debounce.update(kUnderMps, kStartS + kStepS);
  debounce.update(kOverMps, kStartS + kLimits.sustain_s);

  EXPECT_EQ(debounce.level(), TrustLevel::kTrusted);
}

TEST(Debounce, RecoversOnlyAfterTheSameSustainPeriod)
{
  Debounce debounce(kLimits);

  debounce.update(kOverMps, kStartS);
  debounce.update(kOverMps, kStartS + kLimits.sustain_s);

  const double recovering_s = kStartS + kLimits.sustain_s + kStepS;

  debounce.update(kUnderMps, recovering_s);
  debounce.update(kUnderMps, recovering_s + kLimits.sustain_s - kStepS);
  EXPECT_EQ(debounce.level(), TrustLevel::kSuspect);

  debounce.update(kUnderMps, recovering_s + kLimits.sustain_s);
  EXPECT_EQ(debounce.level(), TrustLevel::kTrusted);
}

}  // namespace
}  // namespace quorum_monitors
