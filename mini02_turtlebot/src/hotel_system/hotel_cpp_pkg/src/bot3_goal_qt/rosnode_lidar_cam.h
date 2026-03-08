#ifndef ROSNODE_LIDAR_CAM_H
#define ROSNODE_LIDAR_CAM_H

#include <QWidget>
/* 서울기술교육센터 IoT/Embedded/Robot/AI 교육 실습자료*/
/* 작성자 : SHKIM */
#include <QTimer>
#include <QLabel>
#include <chrono>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <image_transport/image_transport.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <cv_bridge/cv_bridge.h>

class RosNodeLidarCam : public QWidget
{
    Q_OBJECT
private:
    rclcpp::Node::SharedPtr node_lidar_cam;
    rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr subscription_lidar;
    void scan_callback(const sensor_msgs::msg::LaserScan::SharedPtr);

    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr subscription_cam;
    void image_callback(const sensor_msgs::msg::Image::SharedPtr) const;


public:
    explicit RosNodeLidarCam(QWidget *parent = nullptr);
    ~RosNodeLidarCam();
    QLabel* pLcamView;
    rclcpp::Node::SharedPtr getNode();
signals:
    void ldsReceiveSig(float *);
private slots:
    void OnTimerCallbackFunc(void);
};

#endif // ROSNODE_LIDAR_CAM_H
