#include <exception>
#include <expected>
#include <memory>
#include <optional>
#include <string_view>

#include "px4_msgs/msg/sensor_gps.hpp"
#include "rclcpp/rclcpp.hpp"

#include "quorum_monitors/gnss_monitor.hpp"

namespace quorum_monitors {

// MPC_XY_VEL_MAX, the airframe's own horizontal ceiling
constexpr double kKinematicLimitMps = 12.0;

// The baseline a 30 Hz fix stream needs for motion to outweigh noise
constexpr double kMinIntervalS = 1.0;

// Spans a px4 catch-up burst while a 50 m step still reads above the limit
constexpr double kMaxIntervalS = 4.0;

constexpr int kWarnThrottleMs = 1000;

class GnssMonitorNode final : public rclcpp::Node {
public:
  GnssMonitorNode() : Node("gnss_monitor")
  {
    limits_ = GateLimits{.limit_mps = kKinematicLimitMps,
                         .min_interval_s = kMinIntervalS,
                         .max_interval_s = kMaxIntervalS};

    subscription_ = this->create_subscription<px4_msgs::msg::SensorGps>(
        "/quorum/vehicle_gps_position", rclcpp::SensorDataQoS(),
        [this](const px4_msgs::msg::SensorGps& msg) { on_fix(msg); });
  }

private:
  void on_fix(const px4_msgs::msg::SensorGps& msg)
  {
    const FixSample current{.position = GeoPosition{.latitude_deg = msg.latitude_deg,
                                                    .longitude_deg = msg.longitude_deg},
                            .time_s = seconds_from_micros(msg.timestamp)};

    if (!reference_) {
      reference_ = current;
      return;
    }

    const std::expected<GateVerdict, GateError> result =
        check_kinematic_gate(*reference_, current, limits_);

    report(result);

    if (advances_reference(result)) {
      reference_ = current;
    }
  }

  void report(const std::expected<GateVerdict, GateError>& result)
  {
    if (result.has_value()) {
      const GateVerdict verdict = result.value();
      if (verdict.exceeded) {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), kWarnThrottleMs,
                             "gps %.1f m/s over %.2f s, limit %.1f", verdict.speed_mps,
                             verdict.dt_s, limits_.limit_mps);
      }
      return;
    }

    switch (result.error()) {
      case GateError::kTooSoon:
        break;

      case GateError::kGap:
        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kWarnThrottleMs,
                             "gps interval too long to judge");
        break;

      case GateError::kUnsetStamp:
      case GateError::kNotAdvanced: {
        const std::string_view reason = to_string(result.error());
        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kWarnThrottleMs, "gps fix rejected: %.*s",
                             static_cast<int>(reason.size()), reason.data());
        break;
      }
    }
  }

  GateLimits limits_{};
  std::optional<FixSample> reference_;
  rclcpp::Subscription<px4_msgs::msg::SensorGps>::SharedPtr subscription_;
};

}  // namespace quorum_monitors

int main(int argc, char* argv[])
{
  try {
    rclcpp::init(argc, argv);
    const auto node = std::make_shared<quorum_monitors::GnssMonitorNode>();

    rclcpp::spin(node);
    rclcpp::shutdown();
  } catch (const std::exception& e) {
    RCLCPP_ERROR(rclcpp::get_logger("gnss_monitor"), "fatal: %s", e.what());
    return 1;
  }
  return 0;
}