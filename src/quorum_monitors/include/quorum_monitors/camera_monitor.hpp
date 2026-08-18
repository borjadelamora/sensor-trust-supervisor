#ifndef QUORUM_MONITORS__CAMERA_MONITOR_HPP_
#define QUORUM_MONITORS__CAMERA_MONITOR_HPP_

#include <cassert>
#include <cmath>
#include <cstdint>
#include <expected>
#include <numeric>
#include <optional>
#include <span>

#include "quorum_monitors/debounce.hpp"
#include "quorum_monitors/sensor_monitor.hpp"

namespace quorum_monitors {

struct CameraSample {
  double mean_level{};
  double time_s{};
};

/// Mean level over every colour channel of a frame. Requires @p frame to be non-empty
[[nodiscard]] inline double frame_mean(std::span<const uint8_t> frame)
{
  assert(!frame.empty());

  return static_cast<double>(std::accumulate(frame.begin(), frame.end(), uint64_t{0})) /
         static_cast<double>(frame.size());
}

/// Watches the camera for frames too dark or too bright to show ground
class CameraMonitor final : public SensorMonitor {
public:
  CameraMonitor(double centre_level, DebounceLimits debounce_limits)
      : centre_level_(centre_level), debounce_(debounce_limits)
  {
    assert(0.0 <= centre_level && centre_level <= 255.0);
  }

  [[nodiscard]] SensorId sensor_id() const noexcept override { return SensorId::kCamera; }

  [[nodiscard]] std::expected<double, ResidualError> on_frame(const CameraSample& camera)
  {
    if (judgement_ && camera.time_s <= judgement_->time_s) {
      return std::unexpected(ResidualError::kNotAdvanced);
    }

    const double residual = std::abs(camera.mean_level - centre_level_);
    debounce_.update(residual, camera.time_s);
    judgement_ = Judgement{.residual = residual, .time_s = camera.time_s};

    return residual;
  }

  [[nodiscard]] TrustLevel level() const noexcept override { return debounce_.level(); }

  [[nodiscard]] std::optional<Judgement> judgement() const noexcept override { return judgement_; }

  void reset() noexcept override
  {
    debounce_.reset();
    judgement_.reset();
  }

private:
  double centre_level_;
  Debounce debounce_;
  std::optional<Judgement> judgement_;
};

}  // namespace quorum_monitors

#endif  // QUORUM_MONITORS__CAMERA_MONITOR_HPP_
