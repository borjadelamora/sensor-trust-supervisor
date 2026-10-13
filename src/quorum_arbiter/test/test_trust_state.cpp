#include <gtest/gtest.h>

#include "quorum_arbiter/trust_state.hpp"

namespace quorum_arbiter {
namespace {

constexpr TrustLimits kLimits{.debounce_s = 1.0, .ban_s = 2.0, .recover_s = 3.0};

// One fix of the 30 Hz gnss stream
constexpr double kStepS = 1.0 / 30.0;

// Accused from 20 s: suspect at 21, banned at 23
TEST(TrustState, BansOnlyAfterEachWholePeriodAndNeverRecovers)
{
  TrustState state(kLimits);

  state.advance(Evidence::kAccused, 20.0);
  state.advance(Evidence::kAccused, 21.0 - kStepS);
  EXPECT_EQ(state.level(), TrustLevel::kTrusted);

  state.advance(Evidence::kAccused, 21.0);
  state.advance(Evidence::kAccused, 23.0 - kStepS);
  EXPECT_EQ(state.level(), TrustLevel::kSuspect);

  state.advance(Evidence::kAccused, 23.0);
  state.advance(Evidence::kCleared, 24.0);
  state.advance(Evidence::kCleared, 30.0);
  EXPECT_EQ(state.level(), TrustLevel::kBanned);
}

TEST(TrustState, RecoversOnlyAfterTheWholeRecoverPeriod)
{
  TrustState state(kLimits);
  state.advance(Evidence::kAccused, 20.0);
  state.advance(Evidence::kAccused, 21.0);

  state.advance(Evidence::kCleared, 22.0);
  state.advance(Evidence::kCleared, 25.0 - kStepS);
  EXPECT_EQ(state.level(), TrustLevel::kSuspect);

  state.advance(Evidence::kCleared, 25.0);
  EXPECT_EQ(state.level(), TrustLevel::kTrusted);
}

TEST(TrustState, TreatsSilenceAsAnAccusation)
{
  TrustState state(kLimits);

  state.advance(Evidence::kSilent, 20.0);
  state.advance(Evidence::kAccused, 20.5);
  state.advance(Evidence::kSilent, 21.0);

  EXPECT_EQ(state.level(), TrustLevel::kSuspect);
}

}  // namespace
}  // namespace quorum_arbiter
