#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <sstream>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "hotel_interfaces/action/arm_task.hpp"

// TCP
#include <arpa/inet.h>
#include <unistd.h>

using namespace std::chrono_literals;

using ArmTask = hotel_interfaces::action::ArmTask;
using GoalHandleArm = rclcpp_action::ServerGoalHandle<ArmTask>;

static bool tcp_send_and_wait_done(
  const std::string& host,
  int port,
  const std::string& payload,     // 보낼 메시지 (예: "{room_id: ROOM_203}\n")
  std::string& response_out,      // 받은 메시지
  int timeout_sec
){
  int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return false;

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
  ssize_t sent = ::send(sock, payload.c_str(), payload.size(), 0);
  if (sent <= 0) {
    ::close(sock);
    return false;
  }

  // recv (DONE 올 때까지 한 번만 받아도 되는 구조라 1회 recv)
  response_out.clear();
  char buf[1024];
  ssize_t n = ::recv(sock, buf, sizeof(buf) - 1, 0);
  if (n > 0) {
    buf[n] = '\0';
    response_out = std::string(buf);
  }

  ::close(sock);
  return !response_out.empty();
}

// "LOAD_BAG ROOM_203" 같은 command 문자열에서 ROOM_### 뽑기
static std::string extract_room_token(const std::string& cmd)
{
  // 가장 단순하게 "ROOM_" 포함된 토큰을 찾음
  auto pos = cmd.find("ROOM_");
  if (pos == std::string::npos) return "";

  // ROOM_ 다음 숫자들까지 자르기
  size_t end = pos + 5;
  while (end < cmd.size() && (cmd[end] >= '0' && cmd[end] <= '9')) end++;

  return cmd.substr(pos, end - pos); // 예: "ROOM_203"
}

class ArmTaskServerGateway : public rclcpp::Node
{
public:
  ArmTaskServerGateway() : Node("arm_task_server_gateway")
  {
    // TCP 대상(로봇팔/서버)
    this->declare_parameter<std::string>("arm_server_ip", "10.10.141.155");
    this->declare_parameter<int>("arm_server_port", 5006);
    
    // 👇 딱 여기만 10초에서 60초로 수정했습니다! 로봇팔이 짐 다 실을 때까지 넉넉하게 기다려줍니다.
    this->declare_parameter<int>("tcp_timeout_sec", 60);

    server_ = rclcpp_action::create_server<ArmTask>(
      this,
      "arm_task",
      std::bind(&ArmTaskServerGateway::handle_goal, this, std::placeholders::_1, std::placeholders::_2),
      std::bind(&ArmTaskServerGateway::handle_cancel, this, std::placeholders::_1),
      std::bind(&ArmTaskServerGateway::handle_accepted, this, std::placeholders::_1)
    );

    RCLCPP_INFO(get_logger(), "ArmTask Gateway Server ready: /arm_task");
  }

private:
  rclcpp_action::Server<ArmTask>::SharedPtr server_;

  rclcpp_action::GoalResponse handle_goal(
    const rclcpp_action::GoalUUID&,
    std::shared_ptr<const ArmTask::Goal> goal)
  {
    RCLCPP_INFO(get_logger(), "ARM goal: job=%s cmd=%s",
      goal->job_id.c_str(), goal->command.c_str());
    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
  }

  rclcpp_action::CancelResponse handle_cancel(const std::shared_ptr<GoalHandleArm>)
  {
    RCLCPP_WARN(get_logger(), "Cancel requested (not supported in TCP gateway).");
    return rclcpp_action::CancelResponse::ACCEPT;
  }

  void handle_accepted(const std::shared_ptr<GoalHandleArm> gh)
  {
    std::thread{std::bind(&ArmTaskServerGateway::execute, this, gh)}.detach();
  }

  void execute(const std::shared_ptr<GoalHandleArm> gh)
  {
    auto fb  = std::make_shared<ArmTask::Feedback>();
    auto res = std::make_shared<ArmTask::Result>();
    const auto goal = gh->get_goal();

    const std::string ip = this->get_parameter("arm_server_ip").as_string();
    const int port = this->get_parameter("arm_server_port").as_int();
    const int timeout_sec = this->get_parameter("tcp_timeout_sec").as_int();

    // 1) command에서 ROOM_### 추출
    const std::string room_token = extract_room_token(goal->command);
    if (room_token.empty()) {
      res->success = false;
      res->message = "No ROOM_### found in command. Example: 'LOAD_BAG ROOM_203'";
      gh->succeed(res);
      return;
    }

    fb->state = "TCP_CONNECT";
    gh->publish_feedback(fb);

    // 2) 너가 준 소켓 클라처럼 메시지 구성
    //    msg = '"{room_id: ROOM_203}"' 이었는데,
    //    서버가 진짜로 따옴표까지 요구하는지 애매해서
    //    기본은 {room_id: ROOM_203}\n 로 보냅니다.
    //    만약 서버가 따옴표를 요구하면 아래 payload를 '"{...}"\n' 로 바꿔주세요.
    std::string payload = "{room_id: " + room_token + "}\n";

    fb->state = "TCP_SEND";
    gh->publish_feedback(fb);

    std::string response;
    const bool ok = tcp_send_and_wait_done(ip, port, payload, response, timeout_sec);

    if (!ok) {
      res->success = false;
      res->message = "TCP failed: " + ip + ":" + std::to_string(port);
      gh->succeed(res);
      return;
    }

    // 3) DONE 판정 (개행/공백 대비해서 정리)
    auto trim = [](std::string s){
      while (!s.empty() && (s.back()=='\n' || s.back()=='\r' || s.back()==' ' || s.back()=='\t')) s.pop_back();
      size_t i=0;
      while (i<s.size() && (s[i]==' ' || s[i]=='\t' || s[i]=='\n' || s[i]=='\r')) i++;
      return s.substr(i);
    };
    std::string resp = trim(response);

    fb->state = "TCP_RESP";
    gh->publish_feedback(fb);

    if (resp == "DONE") {
      fb->state = "DONE";
      gh->publish_feedback(fb);

      res->success = true;
      res->message = "DONE";
      gh->succeed(res);
    } else {
      res->success = false;
      res->message = "Unexpected response: " + resp;
      gh->succeed(res);
    }
  }
};

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ArmTaskServerGateway>());
  rclcpp::shutdown();
  return 0;
}
