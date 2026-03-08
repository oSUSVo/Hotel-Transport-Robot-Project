#include "rclcpp/rclcpp.hpp"
#include "tb3_hotel_mission/tb3_hotel_mission_node.hpp"

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Tb3HotelMissionNode>());
  rclcpp::shutdown();
  return 0;
}