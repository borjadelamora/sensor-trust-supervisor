#include <cstdint>
#include <exception>
#include <expected>
#include <memory>
#include <string_view>

#include "px4_msgs/msg/sensor_combined.hpp"
#include "px4_msgs/msg/vehicle_attitude.hpp"
#include "px4_msgs/msg/vehicle_optical_flow_vel.hpp"
#include "quorum_msgs/msg/trust_report.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float64.hpp"

#include "quorum_common/px4_time.hpp"

#include "quorum_monitors/flow_monitor.hpp"
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
using quorum_common::VelocityNe;
using quorum_common::VelocityNed;

// Five intervals of the 100 Hz imu
constexpr double kMaxGapS = 0.05;

// The detection window: the residual only shows what changed since the anchor
constexpr double kMaxAgeS = 10.0;

// Five intervals of the 50 Hz attitude stream
constexpr double kMaxAttitudeAgeS = 0.1;

// The residual a flow reading is suspect above, provisional until the benign battery measures it
constexpr double kThresholdMps = 1.0;

// How long the residual has to stay above the threshold, provisional until the same battery
constexpr double kSustainS = 2.0;

class FlowMonitorNode final : public rclcpp::Node {
public:
  FlowMonitorNode()
      : Node("flow_monitor"),
        monitor_(ReferenceLimits{.max_gap_s = kMaxGapS,
                                 .max_age_s = kMaxAgeS,
                                 .max_attitude_age_s = kMaxAttitudeAgeS},
                 DebounceLimits{.threshold = kThresholdMps, .sustain_s = kSustainS})
  {
    residual_publisher_ =
        this->create_publisher<std_msgs::msg::Float64>("/quorum/flow_residual", kQueueDepth);

    trust_publisher_ =
        this->create_publisher<quorum_msgs::msg::TrustReport>("/quorum/trust/flow", kQueueDepth);

    flow_subscription_ = this->create_subscription<px4_msgs::msg::VehicleOpticalFlowVel>(
        "/quorum/vehicle_optical_flow_vel", rclcpp::SensorDataQoS(),
        [this](const px4_msgs::msg::VehicleOpticalFlowVel& msg) { on_flow(msg); });

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

    // Before the first flow sample there is nothing to anchor on, which is not worth reporting
    if (result.has_value() || result.error() == ReferenceError::kNoAnchor ||
        result.error() == ReferenceError::kNoAttitude) {
      return;
    }

    const std::string_view reason = to_string(result.error());
    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs, "imu sample rejected: %.*s",
                         static_cast<int>(reason.size()), reason.data());
  }

  void on_flow(const px4_msgs::msg::VehicleOpticalFlowVel& msg)
  {
    if (!stamped(msg.timestamp)) {
      return;
    }

    const VelocityNe velocity{.north_mps = static_cast<double>(msg.vel_ne[0]),
                              .east_mps = static_cast<double>(msg.vel_ne[1])};

    const std::expected<double, ResidualError> residual_mps = monitor_.on_flow(
        FlowSample{.velocity = velocity, .time_s = seconds_from_micros(msg.timestamp)});

    if (!residual_mps) {
      const std::string_view reason = to_string(residual_mps.error());
      RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs, "flow sample rejected: %.*s",
                           static_cast<int>(reason.size()), reason.data());
      return;
    }

    publish_residual(*residual_mps);
    publish_trust(*residual_mps, msg.timestamp);
    report_level(*residual_mps);
  }

  void publish_residual(double residual_mps)
  {
    std_msgs::msg::Float64 message;
    message.data = residual_mps;

    residual_publisher_->publish(message);
  }

  void publish_trust(double residual_mps, uint64_t stamp_us)
  {
    quorum_msgs::msg::TrustReport message;

    message.stamp = stamp_from_micros(stamp_us);
    message.sensor = quorum_msgs::msg::TrustReport::SENSOR_FLOW;
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
    RCLCPP_WARN(get_logger(), "flow %.*s at %.2f m/s", static_cast<int>(named.size()), named.data(),
                residual_mps);
  }

  FlowMonitor monitor_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr residual_publisher_;
  rclcpp::Publisher<quorum_msgs::msg::TrustReport>::SharedPtr trust_publisher_;
  rclcpp::Subscription<px4_msgs::msg::VehicleOpticalFlowVel>::SharedPtr flow_subscription_;
  rclcpp::Subscription<px4_msgs::msg::SensorCombined>::SharedPtr imu_subscription_;
  rclcpp::Subscription<px4_msgs::msg::VehicleAttitude>::SharedPtr attitude_subscription_;
  LevelTransitions transitions_;
};

}  // namespace quorum_monitors

int main(int argc, char* argv[])
{
  try {
    rclcpp::init(argc, argv);
    const auto node = std::make_shared<quorum_monitors::FlowMonitorNode>();

    rclcpp::spin(node);
    rclcpp::shutdown();
  } catch (const std::exception& e) {
    RCLCPP_ERROR(rclcpp::get_logger("flow_monitor"), "fatal: %s", e.what());
    return 1;
  }
  return 0;
}
