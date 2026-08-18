#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <memory>
#include <string_view>

#include "builtin_interfaces/msg/time.hpp"
#include "quorum_msgs/msg/trust_report.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/float64.hpp"

#include "quorum_monitors/camera_monitor.hpp"
#include "quorum_monitors/sensor_monitor.hpp"
#include "quorum_monitors/trust_publishing.hpp"

namespace quorum_monitors {

// Midway between cruise at 77.8 and the ground at 133.2, over four benign flights
constexpr double kCentreLevel = 105.0;

// Twice the widest benign departure from that centre, 27.7 across the same flights
constexpr double kThresholdLevel = 55.0;

// Twenty-five frames of the 50 Hz camera
constexpr double kSustainS = 0.5;

// The encoding frame_mean's byte arithmetic assumes
constexpr std::string_view kExpectedEncoding = "rgb8";

class CameraMonitorNode final : public rclcpp::Node {
public:
  CameraMonitorNode()
      : Node("camera_monitor"),
        monitor_(kCentreLevel, DebounceLimits{.threshold = kThresholdLevel, .sustain_s = kSustainS})
  {
    residual_publisher_ =
        this->create_publisher<std_msgs::msg::Float64>("/quorum/camera_residual", kQueueDepth);

    trust_publisher_ =
        this->create_publisher<quorum_msgs::msg::TrustReport>("/quorum/trust/camera", kQueueDepth);

    camera_subscription_ = this->create_subscription<sensor_msgs::msg::Image>(
        "/quorum/flow_image", rclcpp::SensorDataQoS(),
        [this](const sensor_msgs::msg::Image& msg) { on_camera(msg); });
  }

private:
  [[nodiscard]] bool stamped(double time_s)
  {
    if (time_s > 0.0) {
      return true;
    }

    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs, "image stamp not set");
    return false;
  }

  // frame_mean walks the rows as one contiguous span, which a padded frame would break
  [[nodiscard]] bool readable(const sensor_msgs::msg::Image& msg)
  {
    if (msg.encoding == kExpectedEncoding &&
        msg.data.size() == static_cast<size_t>(msg.step) * msg.height) {
      return true;
    }

    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs,
                         "frame rejected: %s, %zu bytes", msg.encoding.c_str(), msg.data.size());
    return false;
  }

  void on_camera(const sensor_msgs::msg::Image& msg)
  {
    const double time_s = rclcpp::Time(msg.header.stamp).seconds();

    if (!stamped(time_s)) {
      return;
    }

    if (!readable(msg)) {
      return;
    }

    const double mean_level = frame_mean(msg.data);

    const std::expected<double, ResidualError> residual =
        monitor_.on_frame(CameraSample{.mean_level = mean_level, .time_s = time_s});

    if (!residual) {
      const std::string_view reason = to_string(residual.error());
      RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs,
                           "camera sample rejected: %.*s", static_cast<int>(reason.size()),
                           reason.data());
      return;
    }

    publish_residual(mean_level);
    publish_trust(*residual, msg.header.stamp);
    report_level(*residual);
  }

  void publish_residual(double mean_level)
  {
    std_msgs::msg::Float64 message;
    message.data = mean_level;

    residual_publisher_->publish(message);
  }

  void publish_trust(double residual, const builtin_interfaces::msg::Time& stamp)
  {
    quorum_msgs::msg::TrustReport message;

    message.stamp = stamp;
    message.sensor = quorum_msgs::msg::TrustReport::SENSOR_CAMERA;
    message.level = level_constant(monitor_.level());
    message.residual = residual;

    trust_publisher_->publish(message);
  }

  // Every transition is worth a line, so this one is not throttled
  void report_level(double residual)
  {
    const TrustLevel level = monitor_.level();

    if (!transitions_.changed(level)) {
      return;
    }

    const std::string_view named = to_string(level);
    RCLCPP_WARN(get_logger(), "camera %.*s at %.2f", static_cast<int>(named.size()), named.data(),
                residual);
  }

  CameraMonitor monitor_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr residual_publisher_;
  rclcpp::Publisher<quorum_msgs::msg::TrustReport>::SharedPtr trust_publisher_;
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr camera_subscription_;
  LevelTransitions transitions_;
};

}  // namespace quorum_monitors

int main(int argc, char* argv[])
{
  try {
    rclcpp::init(argc, argv);
    const auto node = std::make_shared<quorum_monitors::CameraMonitorNode>();

    rclcpp::spin(node);
    rclcpp::shutdown();
  } catch (const std::exception& e) {
    RCLCPP_ERROR(rclcpp::get_logger("camera_monitor"), "fatal: %s", e.what());
    return 1;
  }
  return 0;
}
