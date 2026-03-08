cmake_minimum_required(VERSION 3.8)
project(hotel_interfaces)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(builtin_interfaces REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "action/ArmTask.action"
  "action/ElevatorTask.action"
  "action/RoomTask.action"
  "srv/RequestDelivery.srv"
  "srv/RequestGoTo.srv"
  DEPENDENCIES builtin_interfaces
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()