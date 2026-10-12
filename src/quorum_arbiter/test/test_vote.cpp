#include <gtest/gtest.h>

#include <array>

#include "quorum_arbiter/vote.hpp"

namespace quorum_arbiter {
namespace {

// Different per pair so each residual is held to its own limit
constexpr Thresholds kThresholds{.gnss_flow_mps = 1.0, .gnss_ref_mps = 2.0, .flow_ref_mps = 1.5};

using Accused = std::array<bool, kSensorCount>;

struct Row {
  Residuals residuals;
  Outcome outcome;
  Accused accused;
};

TEST(VoteTable, GivesEveryRowItsVerdict)
{
  const std::array<Row, 8> rows{{
      {{0.5, 1.0, 0.5}, Outcome::kAgreement, {false, false, false}},
      {{2.5, 1.0, 0.5}, Outcome::kUnattributed, {true, true, false}},
      {{0.5, 3.0, 0.5}, Outcome::kAccused, {true, false, false}},
      {{2.5, 3.0, 0.5}, Outcome::kAccused, {true, false, false}},
      {{0.5, 1.0, 2.0}, Outcome::kAccused, {false, true, false}},
      {{2.5, 1.0, 2.0}, Outcome::kAccused, {false, true, false}},
      {{0.5, 3.0, 2.0}, Outcome::kAccused, {true, true, false}},
      {{2.5, 3.0, 2.0}, Outcome::kAccused, {true, true, false}},
  }};

  for (std::size_t index = 0; index < rows.size(); ++index) {
    SCOPED_TRACE(index);
    const VoteResult result = vote(rows[index].residuals, kThresholds);
    EXPECT_EQ(result.outcome, rows[index].outcome);
    EXPECT_EQ(result.accused, rows[index].accused);
  }
}

// GNSS clears its limit by 1.0 and flow by 0.5, so the margin is flow's
TEST(Vote, TakesTheMarginFromTheWeakerAccusation)
{
  const VoteResult result =
      vote({.gnss_flow_mps = 0.5, .gnss_ref_mps = 3.0, .flow_ref_mps = 2.0}, kThresholds);

  EXPECT_DOUBLE_EQ(result.margin_mps, 0.5);
}

TEST(Vote, FallsBackOnTheAbsolutePairWithoutAReference)
{
  const VoteResult result = vote({.gnss_flow_mps = 2.5}, kThresholds);

  EXPECT_EQ(result.outcome, Outcome::kUnattributed);
  EXPECT_EQ(result.accused, (Accused{true, true, false}));
}

TEST(Vote, TreatsAResidualAtItsThresholdAsAgreement)
{
  const VoteResult result =
      vote({.gnss_flow_mps = 1.0, .gnss_ref_mps = 2.0, .flow_ref_mps = 1.5}, kThresholds);

  EXPECT_EQ(result.outcome, Outcome::kAgreement);
}

}  // namespace
}  // namespace quorum_arbiter
