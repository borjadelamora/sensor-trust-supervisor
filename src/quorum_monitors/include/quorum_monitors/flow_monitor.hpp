#ifndef QUORUM_MONITORS__FLOW_MONITOR_HPP_
#define QUORUM_MONITORS__FLOW_MONITOR_HPP_

#include <cmath>
#include <cstdint>
#include <expected>
#include <optional>

#include "quorum_monitors/debounce.hpp"
#include "quorum_monitors/sensor_monitor.hpp"
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

/// How far flow's velocity sits from the dead-reckoned one
[[nodiscard]] inline double horizontal_residual_mps(const VelocityNe& flow,
                                                    const VelocityNed& reference)
{
  return std::hypot(flow.north_mps - reference.north_mps, flow.east_mps - reference.east_mps);
}

/// Watches optical flow against a reference anchored on the flow it is fed
class FlowMonitor final : public SensorMonitor {
public:
  FlowMonitor(ReferenceLimits reference_limits, DebounceLimits debounce_limits)
      : reference_(reference_limits), debounce_(debounce_limits)
  {}

  [[nodiscard]] SensorId sensor_id() const noexcept override { return SensorId::kFlow; }

  void on_attitude(const AttitudeSample& attitude, uint8_t reset_count) override
  {
    reference_.on_attitude(attitude, reset_count);
  }

  [[nodiscard]] std::expected<VelocityNed, ReferenceError> on_imu(const ImuSample& imu) override
  {
    return reference_.on_imu(imu);
  }

  [[nodiscard]] std::expected<double, ResidualError> on_flow(const FlowSample& flow)
  {
    if (judgement_ && flow.time_s <= judgement_->time_s) {
      return std::unexpected(ResidualError::kNotAdvanced);
    }

    const std::expected<VelocityNed, ReferenceError> held = reference_.velocity(flow.time_s);

    if (!held) {
      // Flow never measured a vertical velocity, so the anchor starts at zero and integrates
      reference_.anchor(
          VelocityNed{.north_mps = flow.velocity.north_mps, .east_mps = flow.velocity.east_mps},
          flow.time_s);

      return std::unexpected(ResidualError::kNoReference);
    }

    const double residual_mps = horizontal_residual_mps(flow.velocity, *held);
    debounce_.update(residual_mps, flow.time_s);
    judgement_ = Judgement{.residual = residual_mps, .time_s = flow.time_s};

    return residual_mps;
  }

  [[nodiscard]] TrustLevel level() const noexcept override { return debounce_.level(); }

  [[nodiscard]] std::optional<Judgement> judgement() const noexcept override { return judgement_; }

  void reset() noexcept override
  {
    reference_.reset();
    debounce_.reset();
    judgement_.reset();
  }

private:
  ReferenceFeed reference_;
  Debounce debounce_;
  std::optional<Judgement> judgement_;
};

}  // namespace quorum_monitors

#endif  // QUORUM_MONITORS__FLOW_MONITOR_HPP_
