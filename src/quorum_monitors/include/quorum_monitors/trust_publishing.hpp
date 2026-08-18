#ifndef QUORUM_MONITORS__TRUST_PUBLISHING_HPP_
#define QUORUM_MONITORS__TRUST_PUBLISHING_HPP_

#include <cstdint>

#include "builtin_interfaces/msg/time.hpp"
#include "quorum_msgs/msg/trust_report.hpp"
#include "rclcpp/rclcpp.hpp"

#include "quorum_monitors/debounce.hpp"

namespace quorum_monitors {

constexpr int kQueueDepth = 10;

constexpr int kLogThrottleMs = 1000;

/// px4 counts microseconds and rclcpp::Time nanoseconds
[[nodiscard]] inline builtin_interfaces::msg::Time stamp_from_micros(uint64_t micros)
{
  return rclcpp::Time{static_cast<int64_t>(micros) * 1000};
}

[[nodiscard]] constexpr uint8_t level_constant(TrustLevel level)
{
  switch (level) {
    case TrustLevel::kTrusted:
      return quorum_msgs::msg::TrustReport::LEVEL_TRUSTED;

    case TrustLevel::kSuspect:
      return quorum_msgs::msg::TrustReport::LEVEL_SUSPECT;
  }

  // Unreachable while the switch is total, and suspect rather than trusted if it stops being
  return quorum_msgs::msg::TrustReport::LEVEL_SUSPECT;
}

/// Holds the level last seen, so a node logs the transitions and not every sample
class LevelTransitions final {
public:
  [[nodiscard]] bool changed(TrustLevel level) noexcept
  {
    if (level == held_) {
      return false;
    }

    held_ = level;
    return true;
  }

private:
  TrustLevel held_{TrustLevel::kTrusted};
};

}  // namespace quorum_monitors

#endif  // QUORUM_MONITORS__TRUST_PUBLISHING_HPP_
