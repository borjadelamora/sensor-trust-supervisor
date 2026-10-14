#include <cmath>
#include <cstdint>
#include <exception>
#include <expected>
#include <memory>
#include <string_view>

#include "geometry_msgs/msg/vector3_stamped.hpp"
#include "px4_msgs/msg/sensor_combined.hpp"
#include "px4_msgs/msg/sensor_gps.hpp"
#include "px4_msgs/msg/vehicle_attitude.hpp"
#include "quorum_msgs/msg/trust_report.hpp"
#include "rclcpp/rclcpp.hpp"

#include "quorum_common/px4_time.hpp"

#include "quorum_monitors/gnss_monitor.hpp"
#include "quorum_monitors/sensor_monitor.hpp"
#include "quorum_monitors/trust_publishing.hpp"

namespace quorum_monitors {

using quorum_common::AccelerationFrd;
using quorum_common::Attitude;
using quorum_common::AttitudeSample;
using quorum_common::ImuSample;
using quorum_common::kUnsetStampUs;
using quorum_common::ReferenceError;
using quorum_common::ReferenceLimits;
using quorum_common::seconds_from_micros;
using quorum_common::VelocityNed;

// MPC_XY_VEL_MAX, the airframe's own horizontal ceiling
constexpr double kKinematicLimitMps = 12.0;

// The baseline a 30 Hz fix stream needs for motion to outweigh noise
constexpr double kMinIntervalS = 1.0;

// Spans a px4 catch-up burst while a 50 m step still reads above the limit
constexpr double kMaxIntervalS = 4.0;

// Five intervals of the 100 Hz imu
constexpr double kMaxGapS = 0.05;

// The detection window: the residual only shows what changed since the anchor
constexpr double kMaxAgeS = 10.0;

// Five intervals of the 50 Hz attitude stream
constexpr double kMaxAttitudeAgeS = 0.1;

// Above the 1.40 m/s benign ceiling EXP-20260815-001 measured, provisional until the battery
constexpr double kThresholdMps = 2.0;

// How long the residual has to stay above the threshold, provisional in the same way
constexpr double kSustainS = 2.0;

class GnssMonitorNode final : public rclcpp::Node {
public:
  GnssMonitorNode()
      : Node("gnss_monitor"),
        monitor_(GateLimits{.limit_mps = kKinematicLimitMps,
                            .min_interval_s = kMinIntervalS,
                            .max_interval_s = kMaxIntervalS},
                 ReferenceLimits{.max_gap_s = kMaxGapS,
                                 .max_age_s = kMaxAgeS,
                                 .max_attitude_age_s = kMaxAttitudeAgeS},
                 DebounceLimits{.threshold = kThresholdMps, .sustain_s = kSustainS})
  {
    residual_publisher_ = this->create_publisher<geometry_msgs::msg::Vector3Stamped>(
        "/quorum/gnss_residual", kQueueDepth);

    trust_publisher_ =
        this->create_publisher<quorum_msgs::msg::TrustReport>("/quorum/trust/gnss", kQueueDepth);

    gps_subscription_ = this->create_subscription<px4_msgs::msg::SensorGps>(
        "/quorum/vehicle_gps_position", rclcpp::SensorDataQoS(),
        [this](const px4_msgs::msg::SensorGps& msg) { on_fix(msg); });

    imu_subscription_ = this->create_subscription<px4_msgs::msg::SensorCombined>(
        "/quorum/sensor_combined", rclcpp::SensorDataQoS(),
        [this](const px4_msgs::msg::SensorCombined& msg) { on_imu(msg); });

    attitude_subscription_ = this->create_subscription<px4_msgs::msg::VehicleAttitude>(
        "/quorum/vehicle_attitude", rclcpp::SensorDataQoS(),
        [this](const px4_msgs::msg::VehicleAttitude& msg) { on_attitude(msg); });
  }

private:
  [[nodiscard]] bool stamped(uint64_t stamp_us)
  {
    if (stamp_us != kUnsetStampUs) {
      return true;
    }

    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs, "px4 stamp not set");
    return false;
  }

  void on_attitude(const px4_msgs::msg::VehicleAttitude& msg)
  {
    if (!stamped(msg.timestamp)) {
      return;
    }

    const Attitude attitude{.w = static_cast<double>(msg.q[0]),
                            .x = static_cast<double>(msg.q[1]),
                            .y = static_cast<double>(msg.q[2]),
                            .z = static_cast<double>(msg.q[3])};

    monitor_.on_attitude(
        AttitudeSample{.attitude = attitude, .time_s = seconds_from_micros(msg.timestamp)},
        msg.quat_reset_counter);
  }

  void on_imu(const px4_msgs::msg::SensorCombined& msg)
  {
    if (!stamped(msg.timestamp)) {
      return;
    }

    const double time_s = seconds_from_micros(msg.timestamp);

    const AccelerationFrd acceleration{
        .forward_mps2 = static_cast<double>(msg.accelerometer_m_s2[0]),
        .right_mps2 = static_cast<double>(msg.accelerometer_m_s2[1]),
        .down_mps2 = static_cast<double>(msg.accelerometer_m_s2[2])};

    const std::expected<VelocityNed, ReferenceError> result =
        monitor_.on_imu(ImuSample{.acceleration = acceleration, .time_s = time_s});

    // Before the first fix there is nothing to anchor on, which is not worth reporting
    if (result.has_value() || result.error() == ReferenceError::kNoAnchor ||
        result.error() == ReferenceError::kNoAttitude) {
      return;
    }

    const std::string_view reason = to_string(result.error());
    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs, "imu sample rejected: %.*s",
                         static_cast<int>(reason.size()), reason.data());
  }

  void on_fix(const px4_msgs::msg::SensorGps& msg)
  {
    if (!stamped(msg.timestamp)) {
      return;
    }

    const FixSample current{.position = GeoPosition{.latitude_deg = msg.latitude_deg,
                                                    .longitude_deg = msg.longitude_deg},
                            .time_s = seconds_from_micros(msg.timestamp)};

    report(monitor_.check_gate(current));
    check_velocity(msg, current.time_s);
  }

  void check_velocity(const px4_msgs::msg::SensorGps& msg, double time_s)
  {
    if (!msg.vel_ned_valid) {
      RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs,
                           "gps fix carries no velocity");
      return;
    }

    const VelocityNed gps_velocity{.north_mps = static_cast<double>(msg.vel_n_m_s),
                                   .east_mps = static_cast<double>(msg.vel_e_m_s),
                                   .down_mps = static_cast<double>(msg.vel_d_m_s)};

    const std::expected<VelocityNed, ResidualError> residual =
        monitor_.check_velocity(gps_velocity, time_s);

    if (!residual) {
      const std::string_view reason = to_string(residual.error());
      RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs, "gps velocity skipped: %.*s",
                           static_cast<int>(reason.size()), reason.data());
      return;
    }

    const double residual_mps = std::hypot(residual->north_mps, residual->east_mps);

    publish_residual(*residual, msg.timestamp);
    publish_trust(residual_mps, msg.timestamp);
    report_level(residual_mps);
  }

  void publish_residual(const VelocityNed& residual, uint64_t stamp_us)
  {
    geometry_msgs::msg::Vector3Stamped message;

    message.header.stamp = stamp_from_micros(stamp_us);
    message.vector.x = residual.north_mps;
    message.vector.y = residual.east_mps;
    message.vector.z = residual.down_mps;

    residual_publisher_->publish(message);
  }

  void publish_trust(double residual_mps, uint64_t stamp_us)
  {
    quorum_msgs::msg::TrustReport message;

    message.stamp = stamp_from_micros(stamp_us);
    message.sensor = quorum_msgs::msg::TrustReport::SENSOR_GNSS;
    message.level = level_constant(monitor_.level());
    message.residual = residual_mps;

    trust_publisher_->publish(message);
  }

  // Every transition is worth a line, so this one is not throttled
  void report_level(double residual_mps)
  {
    const TrustLevel level = monitor_.level();

    if (!transitions_.changed(level)) {
      return;
    }

    const std::string_view named = to_string(level);
    RCLCPP_WARN(get_logger(), "gps %.*s at %.2f m/s", static_cast<int>(named.size()), named.data(),
                residual_mps);
  }

  void report(const std::expected<GateVerdict, GateError>& result)
  {
    if (result.has_value()) {
      const GateVerdict verdict = result.value();
      if (verdict.exceeded) {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs,
                             "gps %.1f m/s over %.2f s, limit %.1f", verdict.speed_mps,
                             verdict.dt_s, kKinematicLimitMps);
      }
      return;
    }

    switch (result.error()) {
      case GateError::kNoPreviousFix:
      case GateError::kTooSoon:
        break;

      case GateError::kGap:
        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs,
                             "gps interval too long to judge");
        break;

      case GateError::kUnsetStamp:
      case GateError::kNotAdvanced: {
        const std::string_view reason = to_string(result.error());
        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs, "gps fix rejected: %.*s",
                             static_cast<int>(reason.size()), reason.data());
        break;
      }
    }
  }

  GnssMonitor monitor_;
  rclcpp::Publisher<geometry_msgs::msg::Vector3Stamped>::SharedPtr residual_publisher_;
  rclcpp::Publisher<quorum_msgs::msg::TrustReport>::SharedPtr trust_publisher_;
  rclcpp::Subscription<px4_msgs::msg::SensorGps>::SharedPtr gps_subscription_;
  rclcpp::Subscription<px4_msgs::msg::SensorCombined>::SharedPtr imu_subscription_;
  rclcpp::Subscription<px4_msgs::msg::VehicleAttitude>::SharedPtr attitude_subscription_;
  LevelTransitions transitions_;
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
