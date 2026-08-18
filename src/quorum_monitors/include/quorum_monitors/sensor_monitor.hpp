#ifndef QUORUM_MONITORS__SENSOR_MONITOR_HPP_
#define QUORUM_MONITORS__SENSOR_MONITOR_HPP_

#include <optional>
#include <string_view>

#include "quorum_monitors/debounce.hpp"

namespace quorum_monitors {

/// One sample's residual, in the unit its own monitor measures
struct Judgement {
  double residual{};
  double time_s{};
};

enum class SensorId { kGnss, kFlow, kCamera };

enum class ResidualError { kNotAdvanced, kNoReference };

[[nodiscard]] constexpr std::string_view to_string(ResidualError error)
{
  switch (error) {
    case ResidualError::kNotAdvanced:
      return "Not Advanced";

    case ResidualError::kNoReference:
      return "No Reference";
  }

  return "Unknown Residual Error";
}

/// The reporting contract every monitor implements
class SensorMonitor {
public:
  SensorMonitor() = default;
  virtual ~SensorMonitor() = default;

  SensorMonitor(const SensorMonitor&) = delete;
  SensorMonitor& operator=(const SensorMonitor&) = delete;

  SensorMonitor(SensorMonitor&&) = delete;
  SensorMonitor& operator=(SensorMonitor&&) = delete;

  [[nodiscard]] virtual SensorId sensor_id() const noexcept = 0;

  [[nodiscard]] virtual TrustLevel level() const noexcept = 0;

  [[nodiscard]] virtual std::optional<Judgement> judgement() const noexcept = 0;

  virtual void reset() noexcept = 0;
};

}  // namespace quorum_monitors

#endif  // QUORUM_MONITORS__SENSOR_MONITOR_HPP_
