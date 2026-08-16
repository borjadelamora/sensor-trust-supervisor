#include <cstdint>
#include <exception>
#include <expected>
#include <memory>
#include <optional>
#include <string_view>

#include "geometry_msgs/msg/vector3_stamped.hpp"
#include "px4_msgs/msg/sensor_combined.hpp"
#include "px4_msgs/msg/sensor_gps.hpp"
#include "px4_msgs/msg/vehicle_attitude.hpp"
#include "rclcpp/rclcpp.hpp"

#include "quorum_monitors/gnss_monitor.hpp"
#include "quorum_monitors/px4_time.hpp"

namespace quorum_monitors {

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

// px4 leaves a stamp it has not filled in at zero
constexpr uint64_t kUnsetStampUs = 0;

constexpr int kResidualQueueDepth = 10;

constexpr int kWarnThrottleMs = 1000;

class GnssMonitorNode final : public rclcpp::Node {
public:
  GnssMonitorNode()
      : Node("gnss_monitor"),
        limits_{.limit_mps = kKinematicLimitMps,
                .min_interval_s = kMinIntervalS,
                .max_interval_s = kMaxIntervalS},
        reference_(ReferenceLimits{.max_gap_s = kMaxGapS,
                                   .max_age_s = kMaxAgeS,
                                   .max_attitude_age_s = kMaxAttitudeAgeS})
  {
    residual_publisher_ = this->create_publisher<geometry_msgs::msg::Vector3Stamped>(
        "/quorum/gnss_residual", kResidualQueueDepth);

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

    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kWarnThrottleMs, "px4 stamp not set");
    return false;
  }

  void on_attitude(const px4_msgs::msg::VehicleAttitude& msg)
  {
    if (!stamped(msg.timestamp)) {
      return;
    }

    // An ekf2 reset steps the quaternion, so whatever was integrated through it no longer holds
    if (attitude_reset_count_ && *attitude_reset_count_ != msg.quat_reset_counter) {
      reference_.reset();
    }

    attitude_reset_count_ = msg.quat_reset_counter;

    const Attitude attitude{.w = static_cast<double>(msg.q[0]),
                            .x = static_cast<double>(msg.q[1]),
                            .y = static_cast<double>(msg.q[2]),
                            .z = static_cast<double>(msg.q[3])};

    attitude_ = AttitudeSample{.attitude = attitude, .time_s = seconds_from_micros(msg.timestamp)};
  }

  void on_imu(const px4_msgs::msg::SensorCombined& msg)
  {
    if (!stamped(msg.timestamp)) {
      return;
    }

    // A silent skip here leaves the residual a gps difference
    if (!attitude_) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), kWarnThrottleMs, "no attitude yet");
      return;
    }

    const double time_s = seconds_from_micros(msg.timestamp);

    const AccelerationFrd acceleration{
        .forward_mps2 = static_cast<double>(msg.accelerometer_m_s2[0]),
        .right_mps2 = static_cast<double>(msg.accelerometer_m_s2[1]),
        .down_mps2 = static_cast<double>(msg.accelerometer_m_s2[2])};

    const std::expected<VelocityNed, ReferenceError> result =
        reference_.advance(ImuSample{.acceleration = acceleration, .time_s = time_s}, *attitude_);

    // Before the first fix there is nothing to anchor on, which is not worth reporting
    if (result.has_value() || result.error() == ReferenceError::kNoAnchor) {
      return;
    }

    const std::string_view reason = to_string(result.error());
    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kWarnThrottleMs, "imu sample rejected: %.*s",
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

    check_gate(current);
    check_velocity(msg, current.time_s);
  }

  void check_gate(const FixSample& current)
  {
    if (!last_fix_) {
      last_fix_ = current;
      return;
    }

    const std::expected<GateVerdict, GateError> result =
        check_kinematic_gate(*last_fix_, current, limits_);

    report(result);

    if (advances_reference(result)) {
      last_fix_ = current;
    }
  }

  void check_velocity(const px4_msgs::msg::SensorGps& msg, double time_s)
  {
    if (!msg.vel_ned_valid) {
      RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kWarnThrottleMs,
                           "gps fix carries no velocity");
      return;
    }

    const VelocityNed gps_velocity{.north_mps = static_cast<double>(msg.vel_n_m_s),
                                   .east_mps = static_cast<double>(msg.vel_e_m_s),
                                   .down_mps = static_cast<double>(msg.vel_d_m_s)};

    const std::expected<VelocityNed, ReferenceError> held = reference_.velocity(time_s);

    // An anchor that aged out is replaced by the fix in hand
    if (!held) {
      const std::string_view reason = to_string(held.error());
      RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kWarnThrottleMs, "anchoring on gps: %.*s",
                           static_cast<int>(reason.size()), reason.data());

      reference_.anchor(gps_velocity, time_s);
      return;
    }

    publish_residual(velocity_residual(gps_velocity, *held), msg.timestamp);
  }

  void publish_residual(const VelocityNed& residual, uint64_t stamp_us)
  {
    geometry_msgs::msg::Vector3Stamped message;

    // rclcpp::Time counts nanoseconds
    message.header.stamp = rclcpp::Time{static_cast<int64_t>(stamp_us) * 1000};
    message.vector.x = residual.north_mps;
    message.vector.y = residual.east_mps;
    message.vector.z = residual.down_mps;

    residual_publisher_->publish(message);
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

  GateLimits limits_;
  VelocityReference reference_;
  std::optional<FixSample> last_fix_;
  std::optional<AttitudeSample> attitude_;
  std::optional<uint8_t> attitude_reset_count_;
  rclcpp::Publisher<geometry_msgs::msg::Vector3Stamped>::SharedPtr residual_publisher_;
  rclcpp::Subscription<px4_msgs::msg::SensorGps>::SharedPtr gps_subscription_;
  rclcpp::Subscription<px4_msgs::msg::SensorCombined>::SharedPtr imu_subscription_;
  rclcpp::Subscription<px4_msgs::msg::VehicleAttitude>::SharedPtr attitude_subscription_;
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