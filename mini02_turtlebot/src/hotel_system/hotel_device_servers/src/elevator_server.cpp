// #include <chrono>
// #include <memory>
// #include <string>
// #include <thread>

// #include "rclcpp/rclcpp.hpp"
// #include "rclcpp_action/rclcpp_action.hpp"
// #include "hotel_interfaces/action/elevator_task.hpp"

// using namespace std::chrono_literals;
// using ElevatorTask = hotel_interfaces::action::ElevatorTask;
// using GoalHandleElev = rclcpp_action::ServerGoalHandle<ElevatorTask>;

// class ElevatorTaskServer : public rclcpp::Node {
// public:
//   ElevatorTaskServer() : Node("elevator_task_server") {
//     server_ = rclcpp_action::create_server<ElevatorTask>(
//       this,
//       "elevator_task",
//       std::bind(&ElevatorTaskServer::handle_goal, this, std::placeholders::_1, std::placeholders::_2),
//       std::bind(&ElevatorTaskServer::handle_cancel, this, std::placeholders::_1),
//       std::bind(&ElevatorTaskServer::handle_accepted, this, std::placeholders::_1)
//     );
//     RCLCPP_INFO(get_logger(), "ElevatorTask Action Server ready: /elevator_task");
//   }

// private:
//   rclcpp_action::Server<ElevatorTask>::SharedPtr server_;

//   rclcpp_action::GoalResponse handle_goal(
//     const rclcpp_action::GoalUUID&,
//     std::shared_ptr<const ElevatorTask::Goal> goal)
//   {
//     RCLCPP_INFO(get_logger(), "Goal: job=%s cmd=%s %d->%d",
//       goal->job_id.c_str(), goal->command.c_str(), goal->from_floor, goal->to_floor);
//     return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
//   }

//   rclcpp_action::CancelResponse handle_cancel(const std::shared_ptr<GoalHandleElev>){
//     RCLCPP_WARN(get_logger(), "Cancel requested");
//     return rclcpp_action::CancelResponse::ACCEPT;
//   }

//   void handle_accepted(const std::shared_ptr<GoalHandleElev> gh) {
//     std::thread{std::bind(&ElevatorTaskServer::execute, this, gh)}.detach();
//   }

//   void execute(const std::shared_ptr<GoalHandleElev> gh) {
//     auto fb = std::make_shared<ElevatorTask::Feedback>();
//     auto res = std::make_shared<ElevatorTask::Result>();

//     // (시뮬) 엘리베이터 프로세스
//     fb->state = "WAITING_DOOR";
//     gh->publish_feedback(fb);
//     rclcpp::sleep_for(1200ms);

//     fb->state = "MOVING";
//     gh->publish_feedback(fb);
//     rclcpp::sleep_for(2000ms);

//     fb->state = "ARRIVED";
//     gh->publish_feedback(fb);
//     rclcpp::sleep_for(300ms);

//     res->success = true;
//     res->message = "Elevator arrived: " + std::to_string(gh->get_goal()->to_floor);
//     gh->succeed(res);
//   }
// };

// int main(int argc, char** argv){
//   rclcpp::init(argc, argv);
//   rclcpp::spin(std::make_shared<ElevatorTaskServer>());
//   rclcpp::shutdown();
//   return 0;
// }
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <sstream>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "hotel_interfaces/action/elevator_task.hpp"

// TCP
#include <arpa/inet.h>
#include <unistd.h>

using namespace std::chrono_literals;
using ElevatorTask = hotel_interfaces::action::ElevatorTask;
using GoalHandleElev = rclcpp_action::ServerGoalHandle<ElevatorTask>;

static bool tcp_request(
  const std::string& host, int port,
  const std::string& request_line,   // JSON + '\n'
  std::string& response_line,        // JSON + '\n'
  int timeout_sec = 60
){
  int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return false;

  // timeout
  timeval tv{};
  tv.tv_sec = timeout_sec;
  tv.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    ::close(sock);
    return false;
  }

  if (::connect(sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
    ::close(sock);
    return false;
  }

  // send
  ssize_t sent = ::send(sock, request_line.c_str(), request_line.size(), 0);
  if (sent <= 0) {
    ::close(sock);
    return false;
  }

  // recv until '\n'
  response_line.clear();
  char buf[512];
  while (true) {
    ssize_t n = ::recv(sock, buf, sizeof(buf), 0);
    if (n <= 0) break;
    response_line.append(buf, buf + n);
    if (!response_line.empty() && response_line.back() == '\n') break;
  }

  ::close(sock);
  return !response_line.empty();
}

class ElevatorTaskServer : public rclcpp::Node {
public:
  ElevatorTaskServer() : Node("elevator_task_server_gateway") {
    // 라즈베리 IP/PORT 파라미터
    this->declare_parameter<std::string>("raspi_ip", "10.10.141.70");
    this->declare_parameter<int>("raspi_port", 5005);

    server_ = rclcpp_action::create_server<ElevatorTask>(
      this,
      "elevator_task",
      std::bind(&ElevatorTaskServer::handle_goal, this, std::placeholders::_1, std::placeholders::_2),
      std::bind(&ElevatorTaskServer::handle_cancel, this, std::placeholders::_1),
      std::bind(&ElevatorTaskServer::handle_accepted, this, std::placeholders::_1)
    );

    RCLCPP_INFO(get_logger(), "ElevatorTask Gateway Server: /elevator_task");
  }

private:
  rclcpp_action::Server<ElevatorTask>::SharedPtr server_;

  rclcpp_action::GoalResponse handle_goal(
    const rclcpp_action::GoalUUID&,
    std::shared_ptr<const ElevatorTask::Goal> goal)
  {
    RCLCPP_INFO(get_logger(), "Goal: job=%s cmd=%s %d->%d",
      goal->job_id.c_str(), goal->command.c_str(), goal->from_floor, goal->to_floor);
    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
  }

  rclcpp_action::CancelResponse handle_cancel(const std::shared_ptr<GoalHandleElev>){
    RCLCPP_WARN(get_logger(), "Cancel requested (not supported in TCP gateway).");
    return rclcpp_action::CancelResponse::ACCEPT;
  }

  void handle_accepted(const std::shared_ptr<GoalHandleElev> gh) {
    std::thread{std::bind(&ElevatorTaskServer::execute, this, gh)}.detach();
  }

  void execute(const std::shared_ptr<GoalHandleElev> gh) {
    auto fb = std::make_shared<ElevatorTask::Feedback>();
    auto res = std::make_shared<ElevatorTask::Result>();

    const auto goal = gh->get_goal();
    const std::string ip = this->get_parameter("raspi_ip").as_string();
    const int port = this->get_parameter("raspi_port").as_int();

    // TCP로 보낼 JSON 한 줄
    std::ostringstream oss;
    oss << "{"
        << "\"job_id\":\"" << goal->job_id << "\","
        << "\"command\":\"" << goal->command << "\","
        << "\"from_floor\":" << goal->from_floor << ","
        << "\"to_floor\":" << goal->to_floor
        << "}\n";
    std::string request_line = oss.str();

    fb->state = "SENT_TO_RASPI";
    gh->publish_feedback(fb);

    std::string response_line;
    bool ok = tcp_request(ip, port, request_line, response_line, 60);

    if (!ok) {
      res->success = false;
      res->message = "TCP request failed to raspi(" + ip + ":" + std::to_string(port) + ")";
      gh->succeed(res);
      return;
    }

    // 응답은 JSON이지만 여기서는 간단 파싱(성공/메시지 키워드 검색)만
    bool success = (response_line.find("\"success\": true") != std::string::npos) ||
                   (response_line.find("\"success\":true") != std::string::npos);

    std::string msg = response_line;
    if (msg.size() > 200) msg = msg.substr(0, 200);

    fb->state = "RASPI_RESPONDED";
    gh->publish_feedback(fb);

    res->success = success;
    res->message = msg;
    gh->succeed(res);
  }
};

int main(int argc, char** argv){
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ElevatorTaskServer>());
  rclcpp::shutdown();
  return 0;
}
