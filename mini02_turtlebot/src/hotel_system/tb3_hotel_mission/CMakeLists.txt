cmake_minimum_required(VERSION 3.8)
project(tb3_hotel_mission)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(hotel_interfaces REQUIRED)

include_directories(include)

add_executable(tb3_hotel_mission_node
  src/main.cpp
  src/tb3_hotel_mission_node.cpp
)

ament_target_dependencies(tb3_hotel_mission_node
  rclcpp
  rclcpp_action
  nav2_msgs
  geometry_msgs
  tf2
  tf2_geometry_msgs
  std_msgs
  hotel_interfaces
)

install(TARGETS tb3_hotel_mission_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

ament_package()