#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>

#include "px4_msgs/msg/offboard_control_mode.hpp"
#include "px4_msgs/msg/trajectory_setpoint.hpp"
#include "px4_msgs/msg/vehicle_command.hpp"
#include "px4_msgs/msg/vehicle_local_position.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

namespace quorum_offboard {

// 10 Hz. PX4 drops offboard without a stream for COM_OF_LOSS_T (1 s default).
constexpr auto kTickPeriod = 100ms;

// DO_SET_MODE params: base mode bit 0 = custom, PX4 custom main mode 6 = offboard.
constexpr float kBaseModeCustom = 1.0F;
constexpr float kMainModeOffboard = 6.0F;
constexpr float kArmRequest = 1.0F;

// Float to match the message fields.
constexpr float kHoldAltitudeM = 5.0F;
constexpr float kHoldYawRad = 0.0F;

// PX4 refuses offboard until the setpoint stream is already running.
constexpr uint64_t kSetpointsBeforeArm = 10;

class OffboardNode : public rclcpp::Node {
public:
  OffboardNode() : Node("offboard_node")
  {
    offboard_control_mode_pub_ = this->create_publisher<px4_msgs::msg::OffboardControlMode>(
        "/fmu/in/offboard_control_mode", 2);

    trajectory_setpoint_pub_ =
        this->create_publisher<px4_msgs::msg::TrajectorySetpoint>("/fmu/in/trajectory_setpoint", 2);

    vehicle_command_pub_ =
        this->create_publisher<px4_msgs::msg::VehicleCommand>("/fmu/in/vehicle_command", 2);

    timer_ = this->create_wall_timer(kTickPeriod, [this]() { tick(); });

    subscription_ = this->create_subscription<px4_msgs::msg::VehicleLocalPosition>(
        "/fmu/out/vehicle_local_position_v1", rclcpp::SensorDataQoS(),
        [this](const px4_msgs::msg::VehicleLocalPosition& msg) { topic_callback(msg); });
  }

private:
  [[nodiscard]] uint64_t now_us()
  {
    return static_cast<uint64_t>(this->get_clock()->now().nanoseconds() / 1000);
  }

  void tick()
  {
    publish_offboard_control_mode();
    publish_trajectory_setpoint();

    if (setpoints_sent_ < kSetpointsBeforeArm) {
      ++setpoints_sent_;
    } else if (setpoints_sent_ == kSetpointsBeforeArm) {
      ++setpoints_sent_;
      publish_vehicle_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_MODE,
                              kBaseModeCustom, kMainModeOffboard);
      publish_vehicle_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM,
                              kArmRequest, 0.0F);
    }
  }

  void publish_offboard_control_mode()
  {
    px4_msgs::msg::OffboardControlMode msg{};
    msg.position = true;
    msg.timestamp = now_us();
    offboard_control_mode_pub_->publish(msg);
  }

  void publish_trajectory_setpoint()
  {
    px4_msgs::msg::TrajectorySetpoint msg{};
    msg.position = {0.0F, 0.0F, -kHoldAltitudeM};  // NED, so down is negative
    msg.yaw = kHoldYawRad;
    msg.timestamp = now_us();
    trajectory_setpoint_pub_->publish(msg);
  }

  void publish_vehicle_command(uint16_t command, float param1, float param2)
  {
    px4_msgs::msg::VehicleCommand msg{};
    msg.param1 = param1;
    msg.param2 = param2;
    msg.command = command;
    msg.target_system = 1;
    msg.target_component = 1;
    msg.source_system = 1;
    msg.source_component = 1;
    msg.from_external = true;
    msg.timestamp = now_us();
    vehicle_command_pub_->publish(msg);
  }

  void topic_callback(const px4_msgs::msg::VehicleLocalPosition& msg)
  {
    const double altitude_m = -static_cast<double>(msg.z);
    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "Altitude: '%f'm",
                         altitude_m);
  }

  rclcpp::Publisher<px4_msgs::msg::OffboardControlMode>::SharedPtr offboard_control_mode_pub_;
  rclcpp::Publisher<px4_msgs::msg::TrajectorySetpoint>::SharedPtr trajectory_setpoint_pub_;
  rclcpp::Publisher<px4_msgs::msg::VehicleCommand>::SharedPtr vehicle_command_pub_;
  rclcpp::Subscription<px4_msgs::msg::VehicleLocalPosition>::SharedPtr subscription_;
  rclcpp::TimerBase::SharedPtr timer_;
  uint64_t setpoints_sent_{0};
};
}  // namespace quorum_offboard

int main(int argc, char* argv[])
{
  try {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<quorum_offboard::OffboardNode>());
    rclcpp::shutdown();
  } catch (const std::exception& e) {
    RCLCPP_ERROR(rclcpp::get_logger("offboard_node"), "fatal: %s", e.what());
    return 1;
  }
  return 0;
}