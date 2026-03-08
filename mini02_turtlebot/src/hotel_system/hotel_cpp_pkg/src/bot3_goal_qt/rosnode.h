#ifndef ROSNODE_H
#define ROSNODE_H

#include <QWidget>
#include <QTimer>
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <sensor_msgs/msg/battery_state.hpp>
#include <std_msgs/msg/string.hpp>

using namespace std::chrono_literals;
class RosNode : public QWidget
{
    Q_OBJECT
private:
    geometry_msgs::msg::Twist msg_twist;
    rclcpp::Node::SharedPtr node_teleop; // ROS 노드 객체
    rclcpp::TimerBase::SharedPtr timer_teleop; // 타이머
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_room; //방번호 전송
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_state; // 상태 수신


    void stateCallback(const std_msgs::msg::String::SharedPtr msg); // 상태 수신 콜백
    void subscribe_battery_msg(const sensor_msgs::msg::BatteryState::SharedPtr); // 배터리 상태 수신

public:
    explicit RosNode(QWidget *parent = nullptr);
    ~RosNode();
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_teleop;
    rclcpp::Subscription<sensor_msgs::msg::BatteryState>::SharedPtr sub_battery;
    void RunTeleopPublisher(double, double);
    void sendRoom(int room); //방번호 전송
    void sendMissionMessage(QString mission);

signals:
    void batteryLcdDisplaySig(double, double);
    void robotStateSig(QString); // ROS에서 상태 수신 QT UI로 전달하는 신호

private slots:
    void OnTimerCallbackFunc(void);
};

#endif // ROSNODE_H
