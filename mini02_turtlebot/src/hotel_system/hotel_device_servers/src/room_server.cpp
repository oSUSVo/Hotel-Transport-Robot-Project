// #include <chrono>
// #include <memory>
// #include <string>
// #include <thread>
// #include <sstream>

// #include "rclcpp/rclcpp.hpp"
// #include "rclcpp_action/rclcpp_action.hpp"
// #include "hotel_interfaces/action/room_task.hpp"

// // TCP
// #include <arpa/inet.h>
// #include <unistd.h>

// using namespace std::chrono_literals;

// using RoomTask = hotel_interfaces::action::RoomTask;
// using GoalHandleRoom = rclcpp_action::ServerGoalHandle<RoomTask>;

// static bool tcp_request_line(
//   const std::string& host, int port,
//   const std::string& request_line,   // JSON + '\n'
//   std::string& response_line,        // JSON(or text) + '\n' or just some data
//   int timeout_sec = 8
// ){
//   int sock = ::socket(AF_INET, SOCK_STREAM, 0);
//   if (sock < 0) return false;

//   // timeout
//   timeval tv{};
//   tv.tv_sec = timeout_sec;
//   tv.tv_usec = 0;
//   ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
//   ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

//   sockaddr_in addr{};
//   addr.sin_family = AF_INET;
//   addr.sin_port = htons(static_cast<uint16_t>(port));
//   if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
//     ::close(sock);
//     return false;
//   }

//   if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
//     ::close(sock);
//     return false;
//   }

//   // send
//   ssize_t sent = ::send(sock, request_line.c_str(), request_line.size(), 0);
//   if (sent <= 0) {
//     ::close(sock);
//     return false;
//   }

//   // recv (가능하면 '\n'까지 받기)
//   response_line.clear();
//   char buf[512];
//   while (true) {
//     ssize_t n = ::recv(sock, buf, sizeof(buf), 0);
//     if (n <= 0) break;
//     response_line.append(buf, buf + n);
//     if (!response_line.empty() && response_line.back() == '\n') break;
//   }

//   ::close(sock);
//   return !response_line.empty();
// }

// class RoomTaskServerGateway : public rclcpp::Node
// {
// public:
//   RoomTaskServerGateway() : Node("room_task_server_gateway")
//   {
//     this->declare_parameter<std::string>("raspi_ip", "10.10.141.73");
//     this->declare_parameter<int>("raspi_port", 5005);
//     this->declare_parameter<int>("tcp_timeout_sec", 8);

//     server_ = rclcpp_action::create_server<RoomTask>(
//       this,
//       "room_task",  // ✅ tb3_hotel_mission_node.cpp가 쓰는 이름과 동일
//       std::bind(&RoomTaskServerGateway::handle_goal, this, std::placeholders::_1, std::placeholders::_2),
//       std::bind(&RoomTaskServerGateway::handle_cancel, this, std::placeholders::_1),
//       std::bind(&RoomTaskServerGateway::handle_accepted, this, std::placeholders::_1)
//     );

//     RCLCPP_INFO(get_logger(), "RoomTask Gateway Server ready: /room_task");
//   }

// private:
//   rclcpp_action::Server<RoomTask>::SharedPtr server_;

//   rclcpp_action::GoalResponse handle_goal(
//     const rclcpp_action::GoalUUID&,
//     std::shared_ptr<const RoomTask::Goal> goal)
//   {
//     RCLCPP_INFO(get_logger(),
//       "Room goal: job=%s room=%s cmd=%s wait=%d",
//       goal->job_id.c_str(),
//       goal->room_id.c_str(),
//       goal->command.c_str(),
//       goal->wait_sec
//     );
//     return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
//   }

//   rclcpp_action::CancelResponse handle_cancel(const std::shared_ptr<GoalHandleRoom>)
//   {
//     // TCP gateway라 실제 취소는 어려움. 일단 ACCEPT만.
//     RCLCPP_WARN(get_logger(), "Cancel requested (not supported in TCP gateway).");
//     return rclcpp_action::CancelResponse::ACCEPT;
//   }

//   void handle_accepted(const std::shared_ptr<GoalHandleRoom> gh)
//   {
//     std::thread{std::bind(&RoomTaskServerGateway::execute, this, gh)}.detach();
//   }

//   void execute(const std::shared_ptr<GoalHandleRoom> gh)
//   {
//     auto fb  = std::make_shared<RoomTask::Feedback>();
//     auto res = std::make_shared<RoomTask::Result>();

//     const auto goal = gh->get_goal();

//     const std::string ip = this->get_parameter("raspi_ip").as_string();
//     const int port = this->get_parameter("raspi_port").as_int();
//     const int timeout_sec = this->get_parameter("tcp_timeout_sec").as_int();

//     // 1) 라즈베리에 보낼 JSON(1줄) 생성
//     // // 라즈베리 쪽에서 이 JSON을 파싱해서 BUZZ/LED/WAIT 등을 수행하도록 설계
//     // std::ostringstream oss;
//     // oss << "{"
//     //     << "\"job_id\":\""  << goal->job_id  << "\","
//     //     << "\"room_id\":\"" << goal->room_id << "\","
//     //     << "\"command\":\"" << goal->command << "\","
//     //     << "\"wait_sec\":"  << goal->wait_sec
//     //     << "}\n";
//     // 예: ROOM|201|BUZZ|1.0\n
//     // const std::string request_line = oss.str();
//     std::ostringstream oss;
//     oss << "ROOM|" << goal->room_id << "|" << goal->command << "|" << goal->wait_sec << "\n";
//     const std::string request_line = oss.str();

//     // 2) 피드백: KNOCK(요청 송신)
//     fb->state = "KNOCK";
//     gh->publish_feedback(fb);

//     // 3) TCP 요청
//     std::string response_line;
//     bool ok = tcp_request_line(ip, port, request_line, response_line, timeout_sec);

//     if (!ok) {
//       res->success = false;
//       res->message = "TCP request failed to raspi(" + ip + ":" + std::to_string(port) + ")";
//       gh->succeed(res);
//       return;
//     }

//     // 4) 응답 파싱(간단 키워드)
//     // 라즈베리 응답을 JSON으로 맞추면 elevator와 똑같이 success 판정 가능
//     bool success =
//       (response_line.find("\"success\": true") != std::string::npos) ||
//       (response_line.find("\"success\":true")  != std::string::npos) ||
//       (response_line.find("OK")                != std::string::npos) ||
//       (response_line.find("DONE")              != std::string::npos);

//     // 5) 피드백: WAITING (wait_sec가 있으면)
//     if (goal->wait_sec > 0) {
//       fb->state = "WAITING";
//       gh->publish_feedback(fb);
//       // ⚠️ 여기서 로컬 sleep을 줄지/말지는 설계 선택인데,
//       // 보통은 "라즈베리가 wait를 수행하고 응답을 늦게 주는 방식"이 더 자연스러움.
//       // 그래서 gateway에서는 굳이 sleep 안 넣고, state만 알림.
//     }

//     // 6) 피드백: DONE
//     fb->state = "DONE";
//     gh->publish_feedback(fb);

//     // 7) 결과 반환
//     std::string msg = response_line;
//     if (msg.size() > 300) msg = msg.substr(0, 300);

//     res->success = success;
//     res->message = msg;
//     gh->succeed(res);
//   }
// };

// int main(int argc, char** argv)
// {
//   rclcpp::init(argc, argv);
//   rclcpp::spin(std::make_shared<RoomTaskServerGateway>());
//   rclcpp::shutdown();
//   return 0;
// }
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <sstream>
#include <cctype>

// ROS2
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "hotel_interfaces/action/room_task.hpp"

// TCP
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std::chrono_literals;
using RoomTask = hotel_interfaces::action::RoomTask;
using GoalHandleRoom = rclcpp_action::ServerGoalHandle<RoomTask>;

static bool tcp_request_line(
  const std::string& host, int port,
  const std::string& request_line,    // 반드시 '\n' 포함 권장
  std::string& response_line,         // '\n'까지 읽어서 담음
  int timeout_sec
){
  int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return false;

  // send/recv timeout
  timeval tv{};
  tv.tv_sec = timeout_sec;
  tv.tv_usec = 0;
  ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    ::close(sock);
    return false;
  }

  if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
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

// "ROOM_201" -> 201 / "201" -> 201 / "room203" -> 203
static int parse_room_number(const std::string& room_id_str)
{
  std::string digits;
  for (char c : room_id_str) {
    if (std::isdigit(static_cast<unsigned char>(c))) digits.push_back(c);
  }
  if (digits.empty()) return 0;
  try {
    return std::stoi(digits);
  } catch (...) {
    return 0;
  }
}

class RoomTaskServerGateway : public rclcpp::Node
{
public:
  RoomTaskServerGateway() : Node("room_task_server_gateway")
  {
    // 라즈베리(방) IP/PORT (각 방 라즈베리가 따로면, room_server를 여러 개 띄우거나 room_id별 라우팅 구조로 확장 가능)
    this->declare_parameter<std::string>("raspi_ip", "10.10.141.73");
    this->declare_parameter<int>("raspi_port", 5005);
    this->declare_parameter<int>("tcp_timeout_sec", 60);

    server_ = rclcpp_action::create_server<RoomTask>(
      this,
      "room_task",
      std::bind(&RoomTaskServerGateway::handle_goal, this, std::placeholders::_1, std::placeholders::_2),
      std::bind(&RoomTaskServerGateway::handle_cancel, this, std::placeholders::_1),
      std::bind(&RoomTaskServerGateway::handle_accepted, this, std::placeholders::_1)
    );

    RCLCPP_INFO(get_logger(), "RoomTask Gateway Server: /room_task");
  }

private:
  rclcpp_action::Server<RoomTask>::SharedPtr server_;

  rclcpp_action::GoalResponse handle_goal(
    const rclcpp_action::GoalUUID&,
    std::shared_ptr<const RoomTask::Goal> goal)
  {
    RCLCPP_INFO(
      get_logger(),
      "Goal received: job_id=%s room_id=%s cmd=%s wait_sec=%d",
      goal->job_id.c_str(), goal->room_id.c_str(), goal->command.c_str(), goal->wait_sec
    );

    // room_id 파싱이 안 되면 거절
    const int room_num = parse_room_number(goal->room_id);
    if (room_num <= 0) {
      RCLCPP_ERROR(get_logger(), "Invalid room_id format: %s", goal->room_id.c_str());
      return rclcpp_action::GoalResponse::REJECT;
    }

    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
  }

  rclcpp_action::CancelResponse handle_cancel(const std::shared_ptr<GoalHandleRoom>)
  {
    RCLCPP_WARN(get_logger(), "Cancel requested (TCP gateway: best-effort cancel).");
    return rclcpp_action::CancelResponse::ACCEPT;
  }

  void handle_accepted(const std::shared_ptr<GoalHandleRoom> gh)
  {
    std::thread{std::bind(&RoomTaskServerGateway::execute, this, gh)}.detach();
  }

  void execute(const std::shared_ptr<GoalHandleRoom> gh)
  {
    auto fb  = std::make_shared<RoomTask::Feedback>();
    auto res = std::make_shared<RoomTask::Result>();

    const auto goal = gh->get_goal();
    const std::string ip = this->get_parameter("raspi_ip").as_string();
    const int port = this->get_parameter("raspi_port").as_int();
    const int timeout_sec = this->get_parameter("tcp_timeout_sec").as_int();

    const int room_num = parse_room_number(goal->room_id);
    const std::string cmd = goal->command;

    // 1) WAIT 명령이면 라즈베리 호출 없이 대기만 수행 (원하면 여기서도 ARRIVE를 보내도록 바꿀 수 있음)
    if (cmd == "WAIT") {
      fb->state = "WAITING";
      gh->publish_feedback(fb);

      int w = goal->wait_sec;
      if (w < 0) w = 0;

      for (int i = 0; i < w; i++) {
        if (gh->is_canceling()) {
          res->success = false;
          res->message = "Canceled during WAIT";
          gh->canceled(res);
          return;
        }
        rclcpp::sleep_for(1s);
      }

      fb->state = "DONE";
      gh->publish_feedback(fb);

      res->success = true;
      res->message = "WAIT done: " + std::to_string(w) + " sec";
      gh->succeed(res);
      return;
    }

    // 2) 라즈베리에는 ARRIVE만 보냄 (DELIVER/KNOCK 등도 ARRIVE로 매핑)
    fb->state = "SENT_TO_RASPI";
    gh->publish_feedback(fb);

    std::ostringstream oss;
    oss << "{"
        << "\"job_id\":\""  << goal->job_id << "\","
        << "\"command\":\"" << "ARRIVE" << "\","
        << "\"room_id\":"   << room_num
        << "}\n";
    const std::string request_line = oss.str();

    std::string response_line;
    const bool ok = tcp_request_line(ip, port, request_line, response_line, timeout_sec);

    if (!ok) {
      res->success = false;
      res->message = "TCP request failed to raspi(" + ip + ":" + std::to_string(port) + ")";
      gh->succeed(res);
      return;
    }

    fb->state = "RASPI_RESPONDED";
    gh->publish_feedback(fb);

    // 아주 단순 파싱: "success":true 포함 여부만 체크
    const bool success =
      (response_line.find("\"success\": true") != std::string::npos) ||
      (response_line.find("\"success\":true")  != std::string::npos);

    // 옵션: 도착 후 대기 (예: 문 앞 30초 대기)
    int w = goal->wait_sec;
    if (w < 0) w = 0;

    if (w > 0) {
      fb->state = "WAITING";
      gh->publish_feedback(fb);

      for (int i = 0; i < w; i++) {
        if (gh->is_canceling()) {
          res->success = false;
          res->message = "Canceled during WAIT after ARRIVE";
          gh->canceled(res);
          return;
        }
        rclcpp::sleep_for(1s);
      }
    }

    fb->state = "DONE";
    gh->publish_feedback(fb);

    std::string msg = response_line;
    if (msg.size() > 300) msg = msg.substr(0, 300);

    res->success = success;
    res->message = msg;
    gh->succeed(res);
  }
};

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<RoomTaskServerGateway>());
  rclcpp::shutdown();
  return 0;
}
