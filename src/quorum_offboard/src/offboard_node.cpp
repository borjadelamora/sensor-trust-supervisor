#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <vector>

#include "px4_msgs/msg/offboard_control_mode.hpp"
#include "px4_msgs/msg/trajectory_setpoint.hpp"
#include "px4_msgs/msg/vehicle_command.hpp"
#include "px4_msgs/msg/vehicle_local_position.hpp"
#include "rclcpp/rclcpp.hpp"

#include "quorum_offboard/mission.hpp"

using namespace std::chrono_literals;

namespace quorum_offboard {

// 10 Hz. PX4 drops offboard without a stream for COM_OF_LOSS_T (1 s default).
constexpr auto kTickPeriod = 100ms;

// DO_SET_MODE params: base mode bit 0 = custom, PX4 custom main mode 6 = offboard.
constexpr float kBaseModeCustom = 1.0F;
constexpr float kMainModeOffboard = 6.0F;
constexpr float kArmRequest = 1.0F;

// Float to match the message fields.
constexpr float kHoldYawRad = 0.0F;

// PX4 refuses offboard until the setpoint stream is already running.
constexpr uint64_t kPrimingTicks = 10;

// Horizontal radius within which a waypoint counts as reached.
constexpr double kAcceptanceRadiusM = 1.5;

// Without this the radius trips mid-approach and the corner rounds off.
constexpr double kSettleSpeedMps = 0.5;

// Vertical tolerance for the climb to cruise altitude.
constexpr double kAltitudeToleranceM = 0.5;

// Height above home below which the vehicle counts as down.
constexpr double kLandedAltitudeM = 0.5;

constexpr double kLegLengthM = 20.0;
constexpr double kCruiseAltitudeM = 5.0;
constexpr double kCruiseDownM = -kCruiseAltitudeM;

constexpr int64_t kNsPerUs = 1000;
constexpr uint64_t kLogPeriodMs = 1000;

// MAVLink's own names: what they mean depends on the command.
struct CommandParams {
  float param1{};
  float param2{};
};

[[nodiscard]] std::vector<PositionNed> square_mission_ned()
{
  return {
      {kLegLengthM, 0.0, kCruiseDownM},
      {kLegLengthM, kLegLengthM, kCruiseDownM},
      {0.0, kLegLengthM, kCruiseDownM},
      {0.0, 0.0, kCruiseDownM},
  };
}

// VehicleLocalPosition is already NED: x north, y east, z down.
[[nodiscard]] PositionNed to_position_ned(const px4_msgs::msg::VehicleLocalPosition& msg)
{
  return {msg.x, msg.y, msg.z};
}

[[nodiscard]] std::array<float, 3> to_field(const PositionNed& position)
{
  return {static_cast<float>(position.north_m), static_cast<float>(position.east_m),
          static_cast<float>(position.down_m)};
}

class OffboardNode : public rclcpp::Node {
public:
  enum class State {
    Priming,  // streaming setpoints, not yet in offboard/armed
    Takeoff,  // offboard + arm requested, waiting for confirmation
    Mission,
    Landing,
    Done
  };

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
  [[nodiscard]] uint64_t now_us() const
  {
    return static_cast<uint64_t>(this->now().nanoseconds() / kNsPerUs);
  }

  [[nodiscard]] PositionNed active_target() const
  {
    return (state_ == State::Mission ? mission_.current() : PositionNed{0.0, 0.0, kCruiseDownM});
  }

  // The stream must run while we hold offboard, and must stop once PX4 is landing.
  [[nodiscard]] bool streaming_setpoints() const
  {
    return state_ == State::Priming || state_ == State::Takeoff || state_ == State::Mission;
  }

  void tick()
  {
    if (streaming_setpoints()) {
      publish_offboard_control_mode();
      publish_trajectory_setpoint();
    }

    if (!last_position_) {
      return;
    }
    const PositionNed& position = *last_position_;

    switch (state_) {
      case State::Priming:
        if (priming_ticks_ < kPrimingTicks) {
          ++priming_ticks_;
        } else if (priming_ticks_ == kPrimingTicks) {
          ++priming_ticks_;
          publish_vehicle_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_MODE,
                                  {kBaseModeCustom, kMainModeOffboard});
          publish_vehicle_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM,
                                  {kArmRequest, 0.0F});
          state_ = State::Takeoff;
        }
        break;

      case State::Takeoff:
        if (is_at_altitude(position, kCruiseDownM, kAltitudeToleranceM)) {
          state_ = State::Mission;
        }
        break;

      case State::Mission:
        if (is_within(position, mission_.current(), kAcceptanceRadiusM) &&
            horizontal_speed_mps_ <= kSettleSpeedMps) {
          mission_.advance();
          if (mission_.finished()) {
            state_ = State::Landing;
          }
        }
        break;

      case State::Landing:
        if (!land_commanded_) {
          publish_vehicle_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_NAV_LAND, {});
          land_commanded_ = true;
        } else if (is_at_altitude(position, 0.0, kLandedAltitudeM)) {
          state_ = State::Done;
        }
        break;

      case State::Done:
        rclcpp::shutdown();
        break;
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
    msg.position = to_field(active_target());
    msg.yaw = kHoldYawRad;
    msg.timestamp = now_us();
    trajectory_setpoint_pub_->publish(msg);
  }

  void publish_vehicle_command(uint16_t command, CommandParams params)
  {
    px4_msgs::msg::VehicleCommand msg{};
    msg.param1 = params.param1;
    msg.param2 = params.param2;
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
    last_position_ = to_position_ned(msg);
    horizontal_speed_mps_ = std::hypot(msg.vx, msg.vy);
    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), kLogPeriodMs,
                         "state %d  ned n=%.2f e=%.2f d=%.2f m  v=%.2f m/s",
                         static_cast<int>(state_), last_position_->north_m, last_position_->east_m,
                         last_position_->down_m, horizontal_speed_mps_);
  }

  rclcpp::Publisher<px4_msgs::msg::OffboardControlMode>::SharedPtr offboard_control_mode_pub_;
  rclcpp::Publisher<px4_msgs::msg::TrajectorySetpoint>::SharedPtr trajectory_setpoint_pub_;
  rclcpp::Publisher<px4_msgs::msg::VehicleCommand>::SharedPtr vehicle_command_pub_;
  rclcpp::Subscription<px4_msgs::msg::VehicleLocalPosition>::SharedPtr subscription_;
  rclcpp::TimerBase::SharedPtr timer_;
  uint64_t priming_ticks_{0};
  bool land_commanded_{false};

  std::optional<PositionNed> last_position_;
  double horizontal_speed_mps_{0.0};

  State state_{State::Priming};

  Mission mission_{square_mission_ned()};
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
