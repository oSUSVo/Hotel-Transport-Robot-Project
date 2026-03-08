cmake_minimum_required(VERSION 3.8)
project(hotel_device_servers)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(hotel_interfaces REQUIRED)
find_package(std_srvs REQUIRED)

add_executable(arm_server src/arm_server.cpp)
ament_target_dependencies(arm_server rclcpp rclcpp_action hotel_interfaces)

add_executable(elevator_server src/elevator_server.cpp)
ament_target_dependencies(elevator_server rclcpp rclcpp_action hotel_interfaces)

add_executable(room_server src/room_server.cpp)
ament_target_dependencies(room_server rclcpp rclcpp_action hotel_interfaces)

install(TARGETS arm_server elevator_server room_server
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME})

ament_package()