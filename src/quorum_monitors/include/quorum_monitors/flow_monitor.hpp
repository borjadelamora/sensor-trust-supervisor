#ifndef QUORUM_MONITORS__FLOW_MONITOR_HPP_
#define QUORUM_MONITORS__FLOW_MONITOR_HPP_

#include <cmath>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>

#include "quorum_monitors/debounce.hpp"
#include "quorum_monitors/velocity_reference.hpp"

namespace quorum_monitors {

/// Horizontal velocity as optical flow reports it, having no vertical component to report
struct VelocityNe {
  double north_mps{};
  double east_mps{};
};

struct FlowSample {
  VelocityNe velocity{};
  double time_s{};
};

enum class FlowError { kNotAdvanced, kNoReference };

[[nodiscard]] constexpr std::string_view to_string(FlowError error)
{
  switch (error) {
    case FlowError::kNotAdvanced:
      return "Not Advanced";

    case FlowError::kNoReference:
      return "No Reference";
  }

  return "Unknown Flow Error";
}

/// How far flow's velocity sits from the dead-reckoned one
[[nodiscard]] inline double horizontal_residual_mps(const VelocityNe& flow,
                                                    const VelocityNed& reference)
{
  return std::hypot(flow.north_mps - reference.north_mps, flow.east_mps - reference.east_mps);
}

class FlowMonitor final {
public:
  struct Judgement {
    double residual_mps{};
    double time_s{};
  };

  FlowMonitor(ReferenceLimits reference_limits, DebounceLimits debounce_limits)
      : reference_(reference_limits), debounce_(debounce_limits)
  {}

  void on_attitude(const AttitudeSample& attitude, uint8_t reset_count)
  {
    if (attitude_ && attitude_->reset_count != reset_count) {
      reference_.reset();
    }

    attitude_ = AttitudeState{.sample = attitude, .reset_count = reset_count};
  }

  [[nodiscard]] std::expected<VelocityNed, ReferenceError> on_imu(const ImuSample& imu)
  {
    if (!attitude_) {
      return std::unexpected(ReferenceError::kNoAttitude);
    }

    return reference_.advance(imu, attitude_->sample);
  }

  [[nodiscard]] std::expected<double, FlowError> on_flow(const FlowSample& flow)
  {
    if (judgement_ && flow.time_s <= judgement_->time_s) {
      return std::unexpected(FlowError::kNotAdvanced);
    }

    const std::expected<VelocityNed, ReferenceError> held = reference_.velocity(flow.time_s);

    if (!held) {
      // Flow never measured a vertical velocity, so the anchor starts at zero and integrates
      reference_.anchor(
          VelocityNed{.north_mps = flow.velocity.north_mps, .east_mps = flow.velocity.east_mps},
          flow.time_s);

      return std::unexpected(FlowError::kNoReference);
    }

    const double residual_mps = horizontal_residual_mps(flow.velocity, *held);
    debounce_.update(residual_mps, flow.time_s);
    judgement_ = Judgement{.residual_mps = residual_mps, .time_s = flow.time_s};

    return residual_mps;
  }

  [[nodiscard]] TrustLevel level() const noexcept { return debounce_.level(); }

  [[nodiscard]] std::optional<Judgement> judgement() const noexcept { return judgement_; }

private:
  struct AttitudeState {
    AttitudeSample sample{};
    uint8_t reset_count{};
  };

  VelocityReference reference_;
  Debounce debounce_;
  std::optional<AttitudeState> attitude_;
  std::optional<Judgement> judgement_;
};

}  // namespace quorum_monitors

#endif  // QUORUM_MONITORS__FLOW_MONITOR_HPP_
