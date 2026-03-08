#ifndef ROSNODE_ACTION_H
#define ROSNODE_ACTION_H

#include <QWidget>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <std_msgs/msg/string.hpp>
#include "hotel_interfaces/srv/request_delivery.hpp"
#include "rosnode_action.h"

using namespace std::placeholders;
class RosNodeAction : public QWidget
{
    Q_OBJECT
public:
    explicit RosNodeAction(QWidget *parent = nullptr);
    ~RosNodeAction();
    rclcpp::Node::SharedPtr node_goal;
    using NavigateToPose = nav2_msgs::action::NavigateToPose;
    using GoalHandleNav = rclcpp_action::ClientGoalHandle<NavigateToPose>;
    void publishState(std::string state);
    void send_goal(float x, float y);
    void sendRoomNumber(int room);
    // void startMission(int room);

     void requestDelivery(const QString &roomNumber);


private:
    rclcpp::Node::SharedPtr node_;
    rclcpp_action::Client<nav2_msgs::action::NavigateToPose>::SharedPtr client_;
    rclcpp_action::Client<NavigateToPose>::SharedPtr client_ptr_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_pub;
    rclcpp::Client<hotel_interfaces::srv::RequestDelivery>::SharedPtr delivery_client_;
    RosNodeAction *pActionNode;

signals:
};

#endif // ROSNODE_ACTION_H
