#ifndef QUORUM_MONITORS__FLOW_MONITOR_HPP_
#define QUORUM_MONITORS__FLOW_MONITOR_HPP_

#include <cstdint>
#include <expected>
#include <optional>

#include "quorum_common/velocity_reference.hpp"

#include "quorum_monitors/debounce.hpp"
#include "quorum_monitors/sensor_monitor.hpp"

namespace quorum_monitors {

struct FlowSample {
  quorum_common::VelocityNe velocity{};
  double time_s{};
};

/// Watches optical flow against a reference anchored on the flow it is fed
class FlowMonitor final : public SensorMonitor {
public:
  FlowMonitor(quorum_common::ReferenceLimits reference_limits, DebounceLimits debounce_limits)
      : reference_(reference_limits), debounce_(debounce_limits)
  {}

  [[nodiscard]] SensorId sensor_id() const noexcept override { return SensorId::kFlow; }

  void on_attitude(const quorum_common::AttitudeSample& attitude, uint8_t reset_count)
  {
    reference_.on_attitude(attitude, reset_count);
  }

  [[nodiscard]] std::expected<quorum_common::VelocityNed, quorum_common::ReferenceError> on_imu(
      const quorum_common::ImuSample& imu)
  {
    return reference_.on_imu(imu);
  }

  [[nodiscard]] std::expected<double, ResidualError> on_flow(const FlowSample& flow)
  {
    if (judgement_ && flow.time_s <= judgement_->time_s) {
      return std::unexpected(ResidualError::kNotAdvanced);
    }

    const std::expected<quorum_common::VelocityNed, quorum_common::ReferenceError> held =
        reference_.velocity(flow.time_s);

    if (!held) {
      // Flow never measured a vertical velocity, so the anchor starts at zero and integrates
      reference_.anchor(quorum_common::VelocityNed{.north_mps = flow.velocity.north_mps,
                                                   .east_mps = flow.velocity.east_mps},
                        flow.time_s);

      return std::unexpected(ResidualError::kNoReference);
    }

    const double residual_mps = quorum_common::horizontal_residual_mps(flow.velocity, *held);
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
  quorum_common::ReferenceFeed reference_;
  Debounce debounce_;
  std::optional<Judgement> judgement_;
};

}  // namespace quorum_monitors

#endif  // QUORUM_MONITORS__FLOW_MONITOR_HPP_
