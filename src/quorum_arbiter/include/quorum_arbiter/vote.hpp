#ifndef QUORUM_ARBITER__VOTE_HPP_
#define QUORUM_ARBITER__VOTE_HPP_

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>

namespace quorum_arbiter {

/// Sensor id for each of the measurements
enum class SensorId : std::size_t { kGnss, kFlow, kCamera };

/// Number of sensors
constexpr std::size_t kSensorCount{3};

static_assert(std::to_underlying(SensorId::kCamera) + 1 == kSensorCount);

/// Type of verdict for the vote
enum class Outcome { kAgreement, kUnattributed, kAccused };

/// Pairwise velocity disagreements
struct Residuals {
  std::optional<double> gnss_flow_mps{};
  std::optional<double> gnss_ref_mps{};
  std::optional<double> flow_ref_mps{};
};

/// Per-pair disagreement limits
struct Thresholds {
  double gnss_flow_mps{};
  double gnss_ref_mps{};
  double flow_ref_mps{};
};

/// Provide the verdict of a vote
struct VoteResult {
  Outcome outcome{};
  std::array<bool, kSensorCount> accused{};
  double margin_mps{};
};

/// Verdict over the three residuals
[[nodiscard]] constexpr VoteResult vote(Residuals residuals, Thresholds thresholds)
{
  assert(!residuals.gnss_flow_mps.has_value() || !std::isnan(*residuals.gnss_flow_mps));
  assert(!residuals.gnss_ref_mps.has_value() || !std::isnan(*residuals.gnss_ref_mps));
  assert(!residuals.flow_ref_mps.has_value() || !std::isnan(*residuals.flow_ref_mps));
  assert(thresholds.gnss_flow_mps > 0.0 && thresholds.gnss_ref_mps > 0.0 &&
         thresholds.flow_ref_mps > 0.0);

  const bool gnss_flow_high =
      residuals.gnss_flow_mps.has_value() && *residuals.gnss_flow_mps > thresholds.gnss_flow_mps;
  const bool gnss_ref_high =
      residuals.gnss_ref_mps.has_value() && *residuals.gnss_ref_mps > thresholds.gnss_ref_mps;
  const bool flow_ref_high =
      residuals.flow_ref_mps.has_value() && *residuals.flow_ref_mps > thresholds.flow_ref_mps;

  if (gnss_ref_high || flow_ref_high) {
    constexpr double kNotAccused{std::numeric_limits<double>::infinity()};
    const double gnss_excess_mps =
        gnss_ref_high ? *residuals.gnss_ref_mps - thresholds.gnss_ref_mps : kNotAccused;
    const double flow_excess_mps =
        flow_ref_high ? *residuals.flow_ref_mps - thresholds.flow_ref_mps : kNotAccused;
    return {.outcome = Outcome::kAccused,
            .accused = {gnss_ref_high, flow_ref_high, false},
            .margin_mps = std::min(gnss_excess_mps, flow_excess_mps)};
  }
  if (gnss_flow_high) {
    return {.outcome = Outcome::kUnattributed,
            .accused = {true, true, false},
            .margin_mps = *residuals.gnss_flow_mps - thresholds.gnss_flow_mps};
  }
  return {.outcome = Outcome::kAgreement, .accused = {false, false, false}, .margin_mps = 0.0};
}

}  // namespace quorum_arbiter

#endif  // QUORUM_ARBITER__VOTE_HPP_
