#include "tb3_hotel_mission/tb3_hotel_mission_node.hpp"

#include <chrono>
#include <deque>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "tf2/LinearMath/Quaternion.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"
#include "hotel_interfaces/srv/request_go_to.hpp"

using namespace std::chrono_literals;

static int floor_of(const std::string& loc)
{
  if (loc.find("ELEVATOR_2F") != std::string::npos) return 2;
  if (loc.rfind("ROOM_2", 0) == 0) return 2;
  return 1;
}

const char* Tb3HotelMissionNode::step_name(Step s)
{
  switch (s) {
    case Step::IDLE: return "IDLE";
    case Step::NAV_TO_ARM: return "NAV_TO_ARM";
    case Step::ARM_LOAD: return "ARM_LOAD";
    case Step::NAV_TO_ELEV_1F_FRONT: return "NAV_TO_ELEV_1F_FRONT";
    case Step::ELEV_CALL_1F: return "ELEV_CALL_1F";
    case Step::NAV_TO_ELEV_INSIDE_1F: return "NAV_TO_ELEV_INSIDE_1F";
    case Step::ELEV_GO_2F: return "ELEV_GO_2F";
    case Step::NAV_TO_ELEV_2F_FRONT: return "NAV_TO_ELEV_2F_FRONT";
    case Step::NAV_TO_ROOM: return "NAV_TO_ROOM";
    case Step::ROOM_DELIVER_WAIT: return "ROOM_DELIVER_WAIT";
    case Step::NAV_BACK_TO_ELEV_2F_FRONT: return "NAV_BACK_TO_ELEV_2F_FRONT";
    case Step::ELEV_CALL_2F: return "ELEV_CALL_2F";
    case Step::NAV_TO_ELEV_INSIDE_2F: return "NAV_TO_ELEV_INSIDE_2F";
    case Step::ELEV_GO_1F: return "ELEV_GO_1F";
    case Step::NAV_TO_ELEV_1F_FRONT_BACK: return "NAV_TO_ELEV_1F_FRONT_BACK";
    case Step::NAV_TO_WAIT: return "NAV_TO_WAIT";
    case Step::DIRECT_NAV_TO_TARGET: return "DIRECT_NAV_TO_TARGET";
    case Step::DIRECT_NAV_TO_ELEV_FRONT: return "DIRECT_NAV_TO_ELEV_FRONT";
    case Step::DIRECT_ELEV_CALL: return "DIRECT_ELEV_CALL";
    case Step::DIRECT_NAV_TO_ELEV_INSIDE: return "DIRECT_NAV_TO_ELEV_INSIDE";
    case Step::DIRECT_ELEV_GO: return "DIRECT_ELEV_GO";
    case Step::DIRECT_NAV_TO_ELEV_FRONT_AFTER: return "DIRECT_NAV_TO_ELEV_FRONT_AFTER";
    default: return "UNKNOWN";
  }
}

Tb3HotelMissionNode::Tb3HotelMissionNode()
: Node("tb3_hotel_mission")
{
  nav_client_  = rclcpp_action::create_client<Nav>(this, "navigate_to_pose");
  arm_client_  = rclcpp_action::create_client<ArmTask>(this, "arm_task");
  elev_client_ = rclcpp_action::create_client<ElevatorTask>(this, "elevator_task");
  room_client_ = rclcpp_action::create_client<RoomTask>(this, "room_task");

  mission_state_pub_ = this->create_publisher<std_msgs::msg::String>("/mission_state", 10);
  publish_state("BOOT");

  req_srv_ = this->create_service<RequestDelivery>(
    "request_delivery",
    std::bind(&Tb3HotelMissionNode::on_request_delivery, this, std::placeholders::_1, std::placeholders::_2)
  );

  goto_srv_ = this->create_service<RequestGoTo>(
    "request_goto",
    std::bind(&Tb3HotelMissionNode::on_request_goto, this, std::placeholders::_1, std::placeholders::_2)
  );

  load_locations_from_params();
  timer_ = this->create_wall_timer(200ms, std::bind(&Tb3HotelMissionNode::on_timer, this));
  current_floor_ = 1;

  RCLCPP_INFO(get_logger(), "tb3_hotel_mission started. Services: /request_delivery, /request_goto");
}

void Tb3HotelMissionNode::load_locations_from_params()
{
  locations_.clear();

  // 1. HOME & 11. 복귀 (절대 영점)
  locations_["WAIT_LOCATION"]      = {0.000, 0.000, 0.000};  

  // 2. 로봇팔 (ARM) - 🛠 우측 1cm 조정 완료
  locations_["ARM"]                = {0.252, 0.883, -0.151}; 

  // 3. 1층 상행 탑승 대기
  locations_["ELEVATOR_1F_FRONT"]  = {1.048, 0.715, 0.038};

  // 4. 1층 상행 터널 안
  locations_["ELEVATOR_INSIDE_1F"] = {1.533, 0.749, 0.007};  
  
  // 5. 2층 하차 위치
  locations_["ELEVATOR_2F_OUT"]    = {1.993, 0.713, -0.054};
  
  // 6~8. 객실
  locations_["ROOM_201"]           = {1.814, -0.012, -1.540};
  locations_["ROOM_202"]           = {2.352, -0.055, -1.540};
  locations_["ROOM_203"]           = {2.890, -0.098, -1.540};

  // 9. 2층 하행 탑승 대기
  locations_["ELEVATOR_2F_FRONT"]  = {1.909, 0.718, 3.122};  

  // 10. 2층 하행 터널 안
  locations_["ELEVATOR_INSIDE_2F"] = {1.359, 0.753, 3.099};

  // 11. 1층 하차 위치
  locations_["ELEVATOR_1F_OUT"]    = {0.936, 0.770, 3.109}; 

  RCLCPP_WARN(get_logger(), "Loaded coordinates: ARM shifted right 1cm to y=0.883.");
}

geometry_msgs::msg::PoseStamped Tb3HotelMissionNode::make_pose(const std::string& name)
{
  if (locations_.count(name) == 0) {
    throw std::runtime_error("Unknown location: " + name);
  }

  const auto& v = locations_.at(name);
  double x = v[0], y = v[1], yaw = v[2];

  geometry_msgs::msg::PoseStamped pose;
  pose.header.frame_id = "map";
  pose.header.stamp = now();
  pose.pose.position.x = x;
  pose.pose.position.y = y;

  tf2::Quaternion q;
  q.setRPY(0, 0, yaw);
  pose.pose.orientation = tf2::toMsg(q);

  return pose;
}

void Tb3HotelMissionNode::set_step(Step s)
{
  step_ = s;
  publish_state(std::string("STEP=") + step_name(step_) + " job=" + current_job_id_ + " room=" + current_room_ + " goto=" + goto_target_ + " floor=" + std::to_string(current_floor_));
  RCLCPP_INFO(get_logger(), "==> STEP: %s (job=%s room=%s goto=%s floor=%d)", step_name(step_), current_job_id_.c_str(), current_room_.c_str(), goto_target_.c_str(), current_floor_);
}

//void Tb3HotelMissionNode::on_request_delivery(const std::shared_ptr<RequestDelivery::Request> req, std::shared_ptr<RequestDelivery::Response> res)
//{
//  if (locations_.count(req->room_id) == 0) {
//    res->accepted = false;
//    res->message = "Unknown room_id: " + req->room_id;
//    return;
//  }
//  request_queue_.push_back(req->room_id);
//  res->accepted = true;
//  res->message = "Queued delivery: " + req->room_id;
//  RCLCPP_INFO(get_logger(), "Delivery requested: %s", req->room_id.c_str());
//}

void Tb3HotelMissionNode::on_request_delivery(
    const std::shared_ptr<RequestDelivery::Request> req,
    std::shared_ptr<RequestDelivery::Response> res)
{
    // 1. 긴급 정지(EMERGENCY_STOP) 처리
    if (req->room_id == "EMERGENCY_STOP") {
        RCLCPP_WARN(get_logger(), "🚨 EMERGENCY STOP REQUESTED!");

        request_queue_.clear();
        nav_client_->async_cancel_all_goals();
        arm_client_->async_cancel_all_goals();
        elev_client_->async_cancel_all_goals();
        room_client_->async_cancel_all_goals();

        busy_ = false;
        set_step(Step::IDLE);

        res->accepted = true;
        res->message = "Emergency Stop Executed.";
        return;
    }

    // 2. 새로운 명령(ARM, ROOM_xxx, WAIT_LOCATION)이 들어온 경우 즉시 전환 로직
    // 기존에 이동 중이거나 작업 중이더라도 취소하고 새 명령을 수행합니다.
    RCLCPP_INFO(get_logger(), "New delivery request: %s", req->room_id.c_str());

    // 좌표 테이블에 있는지 확인
    if (locations_.count(req->room_id) == 0) {
        res->accepted = false;
        res->message = "Unknown location: " + req->room_id;
        RCLCPP_ERROR(get_logger(), "Unknown location: %s", req->room_id.c_str());
        return;
    }

    // 작업 전환을 위해 기존 목표 취소 및 상태 초기화
    nav_client_->async_cancel_all_goals();
    arm_client_->async_cancel_all_goals();
    elev_client_->async_cancel_all_goals();
    room_client_->async_cancel_all_goals();

    // 큐를 비우고 새 명령만 넣음 (즉시 전환 모드)
    request_queue_.clear();
    request_queue_.push_back(req->room_id);

    // busy_를 false로 일시 설정하여 다음 timer 주기에서 즉시 실행되게 함
    busy_ = false;

    res->accepted = true;
    res->message = "New goal accepted: " + req->room_id;
}


void Tb3HotelMissionNode::on_request_goto(const std::shared_ptr<RequestGoTo::Request> req, std::shared_ptr<RequestGoTo::Response> res)
{
  if (locations_.count(req->target_id) == 0) {
    res->accepted = false;
    res->message = "Unknown target_id: " + req->target_id;
    return;
  }
  goto_queue_.push_back(req->target_id);
  res->accepted = true;
  res->message = "Queued goto: " + req->target_id;
  RCLCPP_INFO(get_logger(), "Goto requested: %s", req->target_id.c_str());
}

void Tb3HotelMissionNode::start_next_job_if_any()
{
  if (busy_) return;

  if (!request_queue_.empty()) {
    current_room_ = request_queue_.front();
    request_queue_.pop_front();

    std::ostringstream oss;
    oss << "job_" << std::fixed << std::setprecision(0) << this->now().seconds();
    current_job_id_ = oss.str();

    goto_target_.clear();
    busy_ = true;

    set_step(Step::NAV_TO_ARM);
    send_nav("ARM");
    return;
  }

  if (!goto_queue_.empty()) {
    goto_target_ = goto_queue_.front();
    goto_queue_.pop_front();

    std::ostringstream oss;
    oss << "job_" << std::fixed << std::setprecision(0) << this->now().seconds();
    current_job_id_ = oss.str();

    current_room_.clear();
    busy_ = true;

    const int target_floor = floor_of(goto_target_);

    if (current_floor_ == target_floor) {
      set_step(Step::DIRECT_NAV_TO_TARGET);
      send_nav(goto_target_);
    } else {
      set_step(Step::DIRECT_NAV_TO_ELEV_FRONT);
      send_nav(current_floor_ == 1 ? "ELEVATOR_1F_FRONT" : "ELEVATOR_2F_FRONT");
    }
    return;
  }
}

void Tb3HotelMissionNode::on_timer()
{
  if (!busy_) {
    start_next_job_if_any();
  }
}

void Tb3HotelMissionNode::send_nav(const std::string& target)
{
  if (!nav_client_->wait_for_action_server(1s)) {
    RCLCPP_ERROR(get_logger(), "Nav2 action server not available: /navigate_to_pose");
    busy_ = false;
    set_step(Step::IDLE);
    return;
  }

  Nav::Goal goal;
  try {
    goal.pose = make_pose(target);
  } catch (const std::exception& e) {
    RCLCPP_ERROR(get_logger(), "make_pose failed: %s", e.what());
    busy_ = false;
    set_step(Step::IDLE);
    return;
  }

  rclcpp_action::Client<Nav>::SendGoalOptions opt;
  opt.result_callback = std::bind(&Tb3HotelMissionNode::on_nav_result, this, std::placeholders::_1);

  (void)nav_client_->async_send_goal(goal, opt);
  RCLCPP_INFO(get_logger(), "NAV goal sent: %s", target.c_str());
}

void Tb3HotelMissionNode::on_nav_result(const GoalHandleNav::WrappedResult& result)
{
  if (result.code != rclcpp_action::ResultCode::SUCCEEDED) {
    RCLCPP_ERROR(get_logger(), "NAV failed at step=%s", step_name(step_));
    busy_ = false;
    set_step(Step::IDLE);
    return;
  }

  switch (step_) {
    case Step::NAV_TO_ARM:
      set_step(Step::ARM_LOAD);
      send_arm("LOAD_BAG");
      break;

    case Step::NAV_TO_ELEV_1F_FRONT:
      set_step(Step::ELEV_CALL_1F);
      send_elevator("CALL", 1, 1);
      break;

    case Step::NAV_TO_ELEV_INSIDE_1F:
      set_step(Step::ELEV_GO_2F);
      send_elevator("GO", 1, 2);
      break;

    case Step::NAV_TO_ELEV_2F_FRONT:
      // 👇 2층 엘베에서 내리면 여기서 3초간 숨을 고릅니다!
      RCLCPP_INFO(get_logger(), "2층 하차 완료! 객실로 가기 전 1초 대기합니다.");
      rclcpp::sleep_for(1s);
      
      set_step(Step::NAV_TO_ROOM);
      send_nav(current_room_);
      break;

    case Step::NAV_TO_ROOM:
      set_step(Step::ROOM_DELIVER_WAIT);
      send_room("DELIVER", 10);
      break;

    case Step::NAV_BACK_TO_ELEV_2F_FRONT:
      set_step(Step::ELEV_CALL_2F);
      send_elevator("CALL", 2, 2);
      break;

    case Step::NAV_TO_ELEV_INSIDE_2F:
      set_step(Step::ELEV_GO_1F);
      send_elevator("GO", 2, 1);
      break;

    case Step::NAV_TO_ELEV_1F_FRONT_BACK:
      // 👇 1층 엘베에서 내려도 여기서 3초간 똑같이 숨을 고릅니다!
      RCLCPP_INFO(get_logger(), "1층 하차 완료! 홈으로 복귀 전 1초 대기합니다.");
      rclcpp::sleep_for(1s);
      
      set_step(Step::NAV_TO_WAIT);
      send_nav("WAIT_LOCATION");
      break;

    case Step::NAV_TO_WAIT:
      RCLCPP_INFO(get_logger(), "Delivery mission finished for room=%s", current_room_.c_str());
      current_floor_ = floor_of("WAIT_LOCATION"); 
      busy_ = false;
      set_step(Step::IDLE);
      break;

    case Step::DIRECT_NAV_TO_TARGET:
      current_floor_ = floor_of(goto_target_);
      RCLCPP_INFO(get_logger(), "Goto finished: target=%s (floor=%d)", goto_target_.c_str(), current_floor_);
      busy_ = false;
      set_step(Step::IDLE);
      break;

    case Step::DIRECT_NAV_TO_ELEV_FRONT:
      set_step(Step::DIRECT_ELEV_CALL);
      send_elevator("CALL", current_floor_, current_floor_);
      break;

    case Step::DIRECT_NAV_TO_ELEV_INSIDE:
      set_step(Step::DIRECT_ELEV_GO);
      send_elevator("GO", current_floor_, floor_of(goto_target_));
      break;

    case Step::DIRECT_NAV_TO_ELEV_FRONT_AFTER:
      set_step(Step::DIRECT_NAV_TO_TARGET);
      send_nav(goto_target_);
      break;

    default:
      RCLCPP_WARN(get_logger(), "Unexpected NAV result in step=%s", step_name(step_));
      busy_ = false;
      set_step(Step::IDLE);
      break;
  }
}
void Tb3HotelMissionNode::send_arm(const std::string& command)
{
  if (!arm_client_->wait_for_action_server(1s)) {
    RCLCPP_ERROR(get_logger(), "Arm action server not available: /arm_task");
    busy_ = false;
    set_step(Step::IDLE);
    return;
  }

  ArmTask::Goal goal;
  goal.job_id = current_job_id_;

  // ✅ 핵심: arm_server가 ROOM_###을 command에서 찾음
  // current_room_ 가 "ROOM_203" 형태라고 가정
  goal.command = command + " " + current_room_;

  rclcpp_action::Client<ArmTask>::SendGoalOptions opt;
  opt.result_callback =
    std::bind(&Tb3HotelMissionNode::on_arm_result, this, std::placeholders::_1);

  (void)arm_client_->async_send_goal(goal, opt);
  RCLCPP_INFO(get_logger(), "ARM task sent: %s", goal.command.c_str());
}

void Tb3HotelMissionNode::on_arm_result(const GoalHandleArm::WrappedResult& result)
{
  // 액션 자체 실패(취소/중단/거절 등)
  if (result.code != rclcpp_action::ResultCode::SUCCEEDED) {
    RCLCPP_ERROR(get_logger(), "ARM action failed (code=%d) at step=%s",
                 static_cast<int>(result.code), step_name(step_));
    busy_ = false;
    set_step(Step::IDLE);
    return;
  }

  // Result 메시지 기반 실패
  if (!result.result || !result.result->success) {
    RCLCPP_ERROR(get_logger(), "ARM task reported failure: %s",
                 result.result ? result.result->message.c_str() : "no result");
    busy_ = false;
    set_step(Step::IDLE);
    return;
  }

  RCLCPP_INFO(get_logger(), "ARM task success: %s", result.result->message.c_str());

  // ✅ 팔 작업 완료 후 다음 스텝 진행
  if (step_ == Step::ARM_LOAD) {
    set_step(Step::NAV_TO_ELEV_1F_FRONT);
    send_nav("ELEVATOR_1F_FRONT");
    return;
  }

  // 예상 밖 상태에서 들어오면 안전하게 종료
  RCLCPP_WARN(get_logger(), "Unexpected ARM result in step=%s", step_name(step_));
  busy_ = false;
  set_step(Step::IDLE);
}

void Tb3HotelMissionNode::send_elevator(const std::string& command, int from_floor, int to_floor)
{
  if (!elev_client_->wait_for_action_server(1s)) {
    RCLCPP_ERROR(get_logger(), "Elevator action server not available: /elevator_task");
    busy_ = false;
    set_step(Step::IDLE);
    return;
  }

  ElevatorTask::Goal goal;
  goal.job_id = current_job_id_;
  goal.command = command;
  goal.from_floor = from_floor;
  goal.to_floor = to_floor;

  rclcpp_action::Client<ElevatorTask>::SendGoalOptions opt;
  opt.result_callback = std::bind(&Tb3HotelMissionNode::on_elevator_result, this, std::placeholders::_1);

  (void)elev_client_->async_send_goal(goal, opt);
  RCLCPP_INFO(get_logger(), "ELEVATOR task sent: %s %d->%d", command.c_str(), from_floor, to_floor);
}

void Tb3HotelMissionNode::on_elevator_result(const GoalHandleElev::WrappedResult& result)
{
  if (result.code != rclcpp_action::ResultCode::SUCCEEDED ||
      !result.result || !result.result->success) {
    RCLCPP_ERROR(get_logger(), "ELEVATOR failed: %s",
                 result.result ? result.result->message.c_str() : "no result");
    busy_ = false;
    set_step(Step::IDLE);
    return;  // ✅ 실패면 여기서 종료 (강행 금지)
  }

  // ✅ 성공했을 때만 다음 단계 진행
  if (step_ == Step::ELEV_CALL_1F) {
    rclcpp::sleep_for(2s);
    set_step(Step::NAV_TO_ELEV_INSIDE_1F);
    send_nav("ELEVATOR_INSIDE_1F");
    return;
  }

  if (step_ == Step::ELEV_GO_2F) {
    rclcpp::sleep_for(2s);
    set_step(Step::NAV_TO_ELEV_2F_FRONT);
    send_nav("ELEVATOR_2F_OUT");
    return;
  }

  if (step_ == Step::ELEV_CALL_2F) {
    rclcpp::sleep_for(2s);
    set_step(Step::NAV_TO_ELEV_INSIDE_2F);
    send_nav("ELEVATOR_INSIDE_2F");
    return;
  }

  if (step_ == Step::ELEV_GO_1F) {
    rclcpp::sleep_for(2s);
    set_step(Step::NAV_TO_ELEV_1F_FRONT_BACK);
    send_nav("ELEVATOR_1F_OUT");
    return;
  }

  if (step_ == Step::DIRECT_ELEV_CALL) {
    rclcpp::sleep_for(2s);
    set_step(Step::DIRECT_NAV_TO_ELEV_INSIDE);
    send_nav(current_floor_ == 1 ? "ELEVATOR_INSIDE_1F" : "ELEVATOR_INSIDE_2F");
    return;
  }

  if (step_ == Step::DIRECT_ELEV_GO) {
    rclcpp::sleep_for(2s);
    current_floor_ = floor_of(goto_target_);
    set_step(Step::DIRECT_NAV_TO_ELEV_FRONT_AFTER);
    send_nav(current_floor_ == 1 ? "ELEVATOR_1F_OUT" : "ELEVATOR_2F_OUT");
    return;
  }

  RCLCPP_WARN(get_logger(), "Unexpected elevator result in step=%s", step_name(step_));
  busy_ = false;
  set_step(Step::IDLE);
}

void Tb3HotelMissionNode::send_room(const std::string& command, int wait_sec)
{
  if (!room_client_->wait_for_action_server(1s)) {
    RCLCPP_ERROR(get_logger(), "Room action server not available: /room_task");
    busy_ = false;
    set_step(Step::IDLE);
    return;
  }

  RoomTask::Goal goal;
  goal.job_id = current_job_id_;
  goal.room_id = current_room_;
  goal.command = command;
  goal.wait_sec = wait_sec;

  rclcpp_action::Client<RoomTask>::SendGoalOptions opt;
  opt.result_callback = std::bind(&Tb3HotelMissionNode::on_room_result, this, std::placeholders::_1);

  (void)room_client_->async_send_goal(goal, opt);
  RCLCPP_INFO(get_logger(), "ROOM task sent: %s wait=%d", current_room_.c_str(), wait_sec);
}


void Tb3HotelMissionNode::on_room_result(const GoalHandleRoom::WrappedResult& result)
{
  if (result.code != rclcpp_action::ResultCode::SUCCEEDED || !result.result->success) {
    RCLCPP_ERROR(get_logger(), "ROOM failed: %s", result.result ? result.result->message.c_str() : "no result");
    busy_ = false;
    set_step(Step::IDLE);
    return;
  }

  // 배송 완료 -> 엘베 복귀 시작
  set_step(Step::NAV_BACK_TO_ELEV_2F_FRONT);
  send_nav("ELEVATOR_2F_FRONT");
}
void Tb3HotelMissionNode::publish_state(const std::string& state)
{
  std_msgs::msg::String msg;
  msg.data = state;
  mission_state_pub_->publish(msg);

  RCLCPP_INFO(this->get_logger(), "[MISSION_STATE] %s", state.c_str());
}