#include "rosnode_action.h"
#include <cmath>

// const double WAIT_X = -0.46, WAIT_Y = -0.52;
// const double ARM_X = -0.50, ARM_Y = 0.60;
// const double ELEV1_X = 0.91, ELEV1_Y = 0.31;
// const double ELEVIN_X = 1.53, ELEVIN_Y = 0.29;
// const double ELEV2_X = 1.17, ELEV2_Y = -0.39;
// const double ROOM201_X = 1.74, ROOM201_Y = -0.35;
// const double ROOM202_X = 2.45, ROOM202_Y = -0.34;
// const double ROOM203_X = -0.46, ROOM203_Y = -0.52;

RosNodeAction::RosNodeAction(QWidget *parent)
    : QWidget{parent}
{
    node_goal = rclcpp::Node::make_shared("bot3_goal_qt");
    client_ptr_ = rclcpp_action::create_client<NavigateToPose>(node_goal, "navigate_to_pose");
    state_pub = node_goal->create_publisher<std_msgs::msg::String>("/robot/state", 10);
    delivery_client_ =
        node_goal->create_client<hotel_interfaces::srv::RequestDelivery>(
            "request_delivery");
}

void RosNodeAction::send_goal(float x, float y) {
    if (!this->client_ptr_->wait_for_action_server(std::chrono::seconds(10))) {
        RCLCPP_ERROR(node_goal->get_logger(), "Action server not available after waiting");
        return;
    }

    auto goal_msg = NavigateToPose::Goal();
    goal_msg.pose.header.frame_id = "map";
    goal_msg.pose.header.stamp = node_goal->now();
    goal_msg.pose.pose.position.x = x;
    goal_msg.pose.pose.position.y = y;
    goal_msg.pose.pose.orientation.w = 1.0;

    auto send_goal_options = rclcpp_action::Client<NavigateToPose>::SendGoalOptions();

    // 피드백 콜백 (거리 남음 등 확인 가능)
    send_goal_options.feedback_callback =
        [this](GoalHandleNav::SharedPtr, const std::shared_ptr<const NavigateToPose::Feedback> feedback) {
            RCLCPP_INFO(node_goal->get_logger(), "Distance remaining: %f", feedback->distance_remaining);
        };

    // 결과 콜백
    send_goal_options.result_callback =
        [&](const GoalHandleNav::WrappedResult & result) {
            if (result.code == rclcpp_action::ResultCode::SUCCEEDED) {
                RCLCPP_INFO(node_goal->get_logger(), "Goal reached!");
            } else {
                RCLCPP_ERROR(node_goal->get_logger(), "Goal failed");
            }
        };

    client_ptr_->async_send_goal(goal_msg, send_goal_options);
}

// void RosNodeAction::startMission(int room)
// {
//     publishState("MOVE_ARM");
//     sendGoal(ARM_X, ARM_Y, 0.0);

//     publishState("WAIT_ELEVATOR");
//     sendGoal(ELEV1_X, ELEV1_Y, 0.0);

//     publishState("IN_ELEVATOR");

//     publishState("AT_ROOM");

//     if(room == 201)
//         sendGoal(ROOM201_X, ROOM201_Y, 0.0);
//     else if(room == 202)
//         sendGoal(ROOM202_X, ROOM202_Y, 0.0);
//     else if(room == 203)
//         sendGoal(ROOM203_X, ROOM203_Y, 0.0);

//     publishState("RETURN_HOME");
//     sendGoal(WAIT_X, WAIT_Y, 0.0);

//     publishState("COMPLETE");

// }

void RosNodeAction::publishState(std::string state)
{
    std_msgs::msg::String msg;
    msg.data = state;
    state_pub->publish(msg);
}

// void RosNodeAction::sendRoomNumber(int room)
//  {
//      std_msgs::msg::String msg;
//      msg.data = std::to_string(room);

//      state_pub->publish(msg);

//      RCLCPP_INFO(node_goal->get_logger(), "Room number sent: %s", msg.data.c_str());
//  }

void RosNodeAction::requestDelivery(const QString &roomNumber)
{
    // node_ 대신 node_goal을 사용해야 안전합니다.
    if (!delivery_client_->wait_for_service(std::chrono::seconds(2))) {
        RCLCPP_ERROR(node_goal->get_logger(), "Service not available"); // node_ -> node_goal
        return;
    }

    auto request = std::make_shared<hotel_interfaces::srv::RequestDelivery::Request>();

    // 숫자가 아닌 "ARM" 같은 문자열이 들어올 경우를 대비한 로직
    std::string room_id;
    if (roomNumber == "ARM" || roomNumber == "WAIT_LOCATION") {
        room_id = roomNumber.toStdString();
    } else {
        room_id = "ROOM_" + roomNumber.toStdString();
    }

    request->room_id = room_id;

    RCLCPP_INFO(node_goal->get_logger(), "Requesting delivery to %s", room_id.c_str());

    // 비동기 요청 후 응답 대기 시 node_goal 사용
    auto future = delivery_client_->async_send_request(request);
    // 주의: GUI 스레드에서 spin_until_future_complete는 프리징을 유발할 수 있으므로
    // 실제 운영 시에는 결과 콜백 방식을 권장합니다.
}

// void RosNodeAction::requestDelivery(const QString &roomNumber)
// {
//     if (!delivery_client_->wait_for_service(std::chrono::seconds(2))) {
//         RCLCPP_ERROR(node_->get_logger(), "Service not available");
//         return;
//     }

//     auto request =
//         std::make_shared<hotel_interfaces::srv::RequestDelivery::Request>();

//     std::string room_id = "ROOM_" + roomNumber.toStdString();
//     request->room_id = room_id;

//     RCLCPP_INFO(node_->get_logger(), "Requesting delivery to %s", room_id.c_str());

//     auto future = delivery_client_->async_send_request(request);

//     // 응답 확인 (선택사항)
//     if (rclcpp::spin_until_future_complete(node_, future)
//         == rclcpp::FutureReturnCode::SUCCESS)
//     {
//         auto response = future.get();
//         RCLCPP_INFO(node_->get_logger(),
//                     "Response: accepted=%d, message=%s",
//                     response->accepted,
//                     response->message.c_str());
//     }
//     else
//     {
//         RCLCPP_ERROR(node_->get_logger(), "Failed to call service");
//     }
// }

 void RosNodeAction::sendRoomNumber(int room)
 {
     if (!delivery_client_->wait_for_service(std::chrono::seconds(2))) {
         RCLCPP_ERROR(node_goal->get_logger(),
                      "request_delivery service not available");
         return;
     }

     auto request =
         std::make_shared<hotel_interfaces::srv::RequestDelivery::Request>();

     request->room_id = "ROOM_" + std::to_string(room);

     delivery_client_->async_send_request(request);

     RCLCPP_INFO(node_goal->get_logger(),
                 "Delivery request sent: %s",
                 request->room_id.c_str());
 }

RosNodeAction::~RosNodeAction()
{
}
