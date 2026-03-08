cmake_minimum_required(VERSION 3.8)
project(hotel_cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(Qt5 REQUIRED COMPONENTS Widgets)
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(hotel_interfaces REQUIRED)

# 캠연동
find_package(cv_bridge REQUIRED)
find_package(image_transport REQUIRED)
find_package(OpenCV REQUIRED)


set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)

include_directories(
  include
  src/bot3_goal_qt
)

# opencv 연동
include_directories(
  ${OpenCV_INCLUDE_DIRS}
)

set(UI_FILES
   src/bot3_goal_qt/mainwidget.ui
 )

set(HEADERS
  src/bot3_goal_qt/mainwidget.h
  src/bot3_goal_qt/rosnode.h
  src/bot3_goal_qt/rosnode_action.h
)

set(SOURCES
  src/bot3_goal_qt/main.cpp
  src/bot3_goal_qt/mainwidget.cpp
  src/bot3_goal_qt/rosnode.cpp
  src/bot3_goal_qt/rosnode_action.cpp
)

add_executable(hotel_gui_node
  ${HEADERS}
  ${SOURCES}
  ${UI_FILES}
  resources.qrc
  src/bot3_goal_qt/rosnode_lidar_cam.h
  src/bot3_goal_qt/rosnode_lidar_cam.cpp
)


if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

#  add_executable(hotel_gui_node
#  src/bot3_goal_qt/main.cpp
#  src/bot3_goal_qt/mainwidget.cpp
#  src/bot3_goal_qt/rosnode.cpp
#  src/bot3_goal_qt/rosnode_action.cpp  
# )

ament_target_dependencies(
    hotel_gui_node
    rclcpp
    rclcpp_action
    geometry_msgs
    sensor_msgs
    nav2_msgs
    std_msgs
    hotel_interfaces
)
target_link_libraries(hotel_gui_node Qt5::Widgets)

ament_target_dependencies(hotel_gui_node cv_bridge image_transport)
target_link_libraries(hotel_gui_node ${OpenCV_LIBRARIES})


install(TARGETS
  hotel_gui_node
  DESTINATION lib/${PROJECT_NAME}
)
ament_package()
