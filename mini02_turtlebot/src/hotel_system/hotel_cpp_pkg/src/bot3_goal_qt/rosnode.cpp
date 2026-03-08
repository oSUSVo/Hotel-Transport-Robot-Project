#include "rosnode.h"

RosNode::RosNode(QWidget *parent)
    : QWidget{parent}
{
    msg_twist = geometry_msgs::msg::Twist();
    auto qos_profile = rclcpp::QoS(rclcpp::KeepLast(10));
    node_teleop = rclcpp::Node::make_shared("teleop_qt");
    pub_teleop = node_teleop->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
    pub_room = node_teleop->create_publisher<std_msgs::msg::String>("/hotel/mission_room", 10);
    sub_battery = node_teleop->create_subscription<sensor_msgs::msg::BatteryState>(
        "battery_state",
        qos_profile,
        std::bind(&RosNode::subscribe_battery_msg, this, std::placeholders::_1));

     sub_state = node_teleop->create_subscription<std_msgs::msg::String>(
         "/mission_state",
         10,
         std::bind(&RosNode::stateCallback, this, std::placeholders::_1)
        );


    QTimer *pQTimer = new QTimer(this);
    connect(pQTimer, SIGNAL(timeout()), this, SLOT(OnTimerCallbackFunc()));
    pQTimer->start(1000);
}
RosNode::~RosNode() {
    rclcpp::shutdown();
}

void RosNode::OnTimerCallbackFunc()
{
    rclcpp::spin_some(node_teleop);
}

void RosNode::subscribe_battery_msg(const sensor_msgs::msg::BatteryState::SharedPtr message) {
//   RCLCPP_INFO(node_teleop->get_logger(), "Received: voltage: '%.2f', percentage: '%.2f'", message->voltage, message->percentage);
    emit batteryLcdDisplaySig(message->voltage, message->percentage);
}


void RosNode::RunTeleopPublisher(double linearX, double angularZ)
{
    msg_twist.linear.x = linearX;
    msg_twist.angular.z = angularZ;
    pub_teleop->publish(msg_twist);
}

void RosNode::stateCallback(const std_msgs::msg::String::SharedPtr msg) // 상태 수신 콜백
{
    emit robotStateSig(QString::fromStdString(msg->data));
}

// void RosNode::sendRoom(int room) // 방번호 전송
// {
//     std_msgs::msg::String msg;
//     msg.data = std::to_string(room);
//     pub_room->publish(msg);
// }


void RosNode::sendMissionMessage(QString mission)
{
    std_msgs::msg::String msg;
    msg.data = mission.toStdString();
    pub_room->publish(msg); // /hotel/mission_room 토픽으로 메시지 전송
}
