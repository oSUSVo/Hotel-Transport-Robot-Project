#pragma once

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"

#include "hotel_interfaces/action/arm_task.hpp"
#include "hotel_interfaces/action/elevator_task.hpp"
#include "hotel_interfaces/action/room_task.hpp"
#include "hotel_interfaces/srv/request_delivery.hpp"

#include <std_msgs/msg/string.hpp>

#include "hotel_interfaces/srv/request_go_to.hpp"

class Tb3HotelMissionNode : public rclcpp::Node {
public:
  using RequestGoTo = hotel_interfaces::srv::RequestGoTo;
  
  Tb3HotelMissionNode();

private:
  // ---------- Types ----------
  using Nav = nav2_msgs::action::NavigateToPose;
  using GoalHandleNav = rclcpp_action::ClientGoalHandle<Nav>;

  using ArmTask = hotel_interfaces::action::ArmTask;
  using GoalHandleArm = rclcpp_action::ClientGoalHandle<ArmTask>;

  using ElevatorTask = hotel_interfaces::action::ElevatorTask;
  using GoalHandleElev = rclcpp_action::ClientGoalHandle<ElevatorTask>;

  using RoomTask = hotel_interfaces::action::RoomTask;
  using GoalHandleRoom = rclcpp_action::ClientGoalHandle<RoomTask>;

  using RequestDelivery = hotel_interfaces::srv::RequestDelivery;

  enum class Step {
    IDLE,
    NAV_TO_ARM,
    ARM_LOAD,
    NAV_TO_ELEV_1F_FRONT,
    ELEV_CALL_1F,
    NAV_TO_ELEV_INSIDE_1F,
    ELEV_GO_2F,
    NAV_TO_ELEV_2F_FRONT,
    NAV_TO_ROOM,
    ROOM_DELIVER_WAIT,
    NAV_BACK_TO_ELEV_2F_FRONT,
    ELEV_CALL_2F,
    NAV_TO_ELEV_INSIDE_2F,
    ELEV_GO_1F,
    NAV_TO_ELEV_1F_FRONT_BACK,
    NAV_TO_WAIT,

    // (기존 배송 Step들...)
    // ---- 직접 이동 Step ----
    DIRECT_NAV_TO_TARGET,
    DIRECT_NAV_TO_ELEV_FRONT,
    DIRECT_ELEV_CALL,
    DIRECT_NAV_TO_ELEV_INSIDE,
    DIRECT_ELEV_GO,
    DIRECT_NAV_TO_ELEV_FRONT_AFTER,
  };

  // ---------- ROS ----------
  rclcpp::TimerBase::SharedPtr timer_;

  rclcpp_action::Client<Nav>::SharedPtr nav_client_;
  rclcpp_action::Client<ArmTask>::SharedPtr arm_client_;
  rclcpp_action::Client<ElevatorTask>::SharedPtr elev_client_;
  rclcpp_action::Client<RoomTask>::SharedPtr room_client_;

  rclcpp::Service<RequestDelivery>::SharedPtr req_srv_;

  // mission_state
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr mission_state_pub_;
  void publish_state(const std::string& state);

  // 직접 이동 서비스
  rclcpp::Service<RequestGoTo>::SharedPtr goto_srv_;

  // ---------- State ----------
  Step step_{Step::IDLE};
  bool busy_{false};

  std::deque<std::string> request_queue_;
  std::string current_room_;
  std::string current_job_id_;

  // locations[name] = {x,y,yaw}
  std::map<std::string, std::vector<double>> locations_;

  // 직접 이동 관련 상태
  std::deque<std::string> goto_queue_;
  std::string goto_target_;
  int current_floor_{1}; // 기본 1층 가정

  // ---------- Helpers ----------
  void on_timer();

  void load_locations_from_params();
  geometry_msgs::msg::PoseStamped make_pose(const std::string& name);

  void start_next_request_if_any();
  void set_step(Step s);

  // Nav
  void send_nav(const std::string& target);
  void on_nav_result(const GoalHandleNav::WrappedResult& result);

  // Arm
  void send_arm(const std::string& command);
  void on_arm_result(const GoalHandleArm::WrappedResult& result);

  // Elevator
  void send_elevator(const std::string& command, int from_floor, int to_floor);
  void on_elevator_result(const GoalHandleElev::WrappedResult& result);

  // Room
  void send_room(const std::string& command, int wait_sec);
  void on_room_result(const GoalHandleRoom::WrappedResult& result);

  // Service callback
  void on_request_delivery(
    const std::shared_ptr<RequestDelivery::Request> req,
    std::shared_ptr<RequestDelivery::Response> res);

  void on_request_goto(
    const std::shared_ptr<RequestGoTo::Request> req,
    std::shared_ptr<RequestGoTo::Response> res);

  void start_next_job_if_any();

  static const char* step_name(Step s);
};