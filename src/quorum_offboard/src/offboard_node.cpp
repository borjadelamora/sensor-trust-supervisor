#include "px4_msgs/msg/vehicle_local_position.hpp"
#include "rclcpp/rclcpp.hpp"

namespace quorum_offboard {
class OffboardNode : public rclcpp::Node {
public:
  OffboardNode() : Node("offboard_node")
  {
    subscription_ = this->create_subscription<px4_msgs::msg::VehicleLocalPosition>(
        "/fmu/out/vehicle_local_position_v1", rclcpp::SensorDataQoS(),
        [this](const px4_msgs::msg::VehicleLocalPosition::ConstSharedPtr& msg) {
          topic_callback(*msg);
        });
  }

private:
  void topic_callback(const px4_msgs::msg::VehicleLocalPosition& msg)
  {
    const double altitude_m = -static_cast<double>(msg.z);
    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "Altitude: '%f'm",
                         altitude_m);
  }
  rclcpp::Subscription<px4_msgs::msg::VehicleLocalPosition>::SharedPtr subscription_;
};
}  // namespace quorum_offboard

int main(int argc, char* argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<quorum_offboard::OffboardNode>());
  rclcpp::shutdown();
  return 0;
}