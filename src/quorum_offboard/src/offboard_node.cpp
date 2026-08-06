#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "px4_msgs/msg/offboard_control_mode.hpp"
#include "px4_msgs/msg/trajectory_setpoint.hpp"
#include "px4_msgs/msg/vehicle_command.hpp"
#include "px4_msgs/msg/vehicle_local_position.hpp"
#include "rclcpp/rclcpp.hpp"

#include "quorum_offboard/mission.hpp"
#include "quorum_offboard/mission_config.hpp"

using namespace std::chrono_literals;

namespace quorum_offboard {

// 10 Hz. PX4 drops offboard without a stream for COM_OF_LOSS_T (1 s default).
constexpr auto kTickPeriod = 100ms;

// Nominal, not measured: runs stay comparable.
constexpr double kTickPeriodS = std::chrono::duration<double>(kTickPeriod).count();

// DO_SET_MODE params: base mode bit 0 = custom, PX4 custom main mode 6 = offboard.
constexpr float kBaseModeCustom = 1.0F;
constexpr float kMainModeOffboard = 6.0F;
constexpr float kArmRequest = 1.0F;

// Float to match the message fields.
constexpr float kHoldYawRad = 0.0F;

// PX4 refuses offboard until the setpoint stream is already running.
constexpr uint64_t kPrimingTicks = 10;

// Without this the radius trips mid-approach and the corner rounds off.
constexpr double kSettleSpeedMps = 0.5;

// Vertical tolerance for the climb to cruise altitude.
constexpr double kAltitudeToleranceM = 0.5;

// Height above home below which the vehicle counts as down.
constexpr double kLandedAltitudeM = 0.5;

constexpr int64_t kNsPerUs = 1000;
constexpr uint64_t kLogPeriodMs = 1000;

// MAVLink's own names: what they mean depends on the command.
struct CommandParams {
  float param1{};
  float param2{};
};

[[nodiscard]] std::expected<MissionSpec, MissionError> read_mission(rclcpp::Node& node)
{
  const std::string frame = node.declare_parameter<std::string>("frame", "");
  const double takeoff_altitude_m = node.declare_parameter<double>("takeoff_altitude_m", 0.0);
  const double cruise_speed_mps = node.declare_parameter<double>("cruise_speed_mps", 0.0);
  const double acceptance_radius_m = node.declare_parameter<double>("acceptance_radius_m", 0.0);
  const double hold_time_s = node.declare_parameter<double>("hold_time_s", 0.0);

  const std::vector<std::string> names =
      node.declare_parameter("waypoints", std::vector<std::string>{});
  std::vector<RawWaypoint> raw_waypoints;
  for (const std::string& name : names) {
    const std::vector<double> position_ned_m =
        node.declare_parameter(name + ".position_ned_m", std::vector<double>{});
    raw_waypoints.push_back(RawWaypoint{name, position_ned_m});
  }

  return make_mission_spec(RawMission{frame, takeoff_altitude_m, cruise_speed_mps,
                                      acceptance_radius_m, hold_time_s, std::move(raw_waypoints)});
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

// Convert height above home to down-positive.
[[nodiscard]] constexpr double to_down_m(double altitude_m) { return -altitude_m; }

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

    subscription_ = this->create_subscription<px4_msgs::msg::VehicleLocalPosition>(
        "/fmu/out/vehicle_local_position_v1", rclcpp::SensorDataQoS(),
        [this](const px4_msgs::msg::VehicleLocalPosition& msg) { topic_callback(msg); });
  }

  // Nothing ticks until a mission is loaded, so a rejected file cannot fly.
  [[nodiscard]] std::expected<void, MissionError> load_mission()
  {
    const std::expected<MissionSpec, MissionError> spec = read_mission(*this);
    if (!spec.has_value()) {
      return std::unexpected(spec.error());
    }

    spec_ = spec.value();
    mission_.emplace(spec_.waypoints);
    commanded_ned_ = PositionNed{0.0, 0.0, to_down_m(spec_.takeoff_altitude_m)};
    hold_ticks_required_ = static_cast<uint64_t>(std::llround(spec_.hold_time_s / kTickPeriodS));
    timer_ = this->create_wall_timer(kTickPeriod, [this]() { tick(); });
    return {};
  }

private:
  [[nodiscard]] uint64_t now_us() const
  {
    return static_cast<uint64_t>(this->now().nanoseconds() / kNsPerUs);
  }

  [[nodiscard]] PositionNed active_target() const
  {
    return (state_ == State::Mission ? commanded_ned_
                                     : PositionNed{0.0, 0.0, to_down_m(spec_.takeoff_altitude_m)});
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
        if (is_at_altitude(position, to_down_m(spec_.takeoff_altitude_m), kAltitudeToleranceM)) {
          state_ = State::Mission;
        }
        break;

      case State::Mission:
        commanded_ned_ = step_towards(commanded_ned_, mission_->current(),
                                      spec_.cruise_speed_mps * kTickPeriodS);

        if (is_within(position, mission_->current(), spec_.acceptance_radius_m) &&
            horizontal_speed_mps_ <= kSettleSpeedMps) {
          if (hold_ticks_ < hold_ticks_required_) {
            ++hold_ticks_;
          } else {
            hold_ticks_ = 0;
            mission_->advance();
            if (mission_->finished()) {
              state_ = State::Landing;
            }
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

  // The setpoint actually streamed, which trails the waypoint at cruise speed.
  PositionNed commanded_ned_{};
  uint64_t hold_ticks_{0};
  uint64_t hold_ticks_required_{0};

  std::optional<PositionNed> last_position_;
  double horizontal_speed_mps_{0.0};

  State state_{State::Priming};

  MissionSpec spec_{};
  std::optional<Mission> mission_;
};
}  // namespace quorum_offboard

int main(int argc, char* argv[])
{
  try {
    rclcpp::init(argc, argv);
    const auto node = std::make_shared<quorum_offboard::OffboardNode>();

    const auto loaded = node->load_mission();
    if (!loaded.has_value()) {
      RCLCPP_ERROR(node->get_logger(), "mission rejected: %s",
                   std::string(quorum_offboard::to_string(loaded.error())).c_str());
      rclcpp::shutdown();
      return 1;
    }

    rclcpp::spin(node);
    rclcpp::shutdown();
  } catch (const std::exception& e) {
    RCLCPP_ERROR(rclcpp::get_logger("offboard_node"), "fatal: %s", e.what());
    return 1;
  }
  return 0;
}
