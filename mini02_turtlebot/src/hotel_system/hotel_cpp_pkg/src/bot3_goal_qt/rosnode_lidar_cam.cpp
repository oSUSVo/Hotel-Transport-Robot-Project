/* 서울기술교육센터 IoT/Embedded/Robot/AI 교육 실습자료*/
/* 작성자 : SHKIM */
#include "rosnode_lidar_cam.h"

RosNodeLidarCam::RosNodeLidarCam(QWidget *parent)
    : QWidget{parent}
{
//    rclcpp::init(0, nullptr);
    auto sensor_qos = rclcpp::QoS(rclcpp::SensorDataQoS());
 //       auto qos_profile = rclcpp::QoS(rclcpp::KeepLast(10));
    node_lidar_cam = rclcpp::Node::make_shared("lidar_cam_qt");
    subscription_lidar = node_lidar_cam->create_subscription<sensor_msgs::msg::LaserScan>(
        "/scan", // The topic name to subscribe to
        sensor_qos,
        std::bind(&RosNodeLidarCam::scan_callback, this, std::placeholders::_1));

    subscription_cam = node_lidar_cam->create_subscription<sensor_msgs::msg::Image>(
//        "image_raw", // The topic name to subscribe to
        "/image_raw",
        10,
        std::bind(&RosNodeLidarCam::image_callback, this, std::placeholders::_1));

//    RCLCPP_INFO(node_lidar_cam->get_logger(), "LaserScan Subscriber Node has started.");

    QTimer *pQTimer = new QTimer(this);
    connect(pQTimer, SIGNAL(timeout()), this, SLOT(OnTimerCallbackFunc()));
    pQTimer->start(10);
}
RosNodeLidarCam::~RosNodeLidarCam()
{
//   rclcpp::shutdown();
}
void RosNodeLidarCam::OnTimerCallbackFunc()
{
    rclcpp::spin_some(node_lidar_cam);
}
void RosNodeLidarCam::scan_callback(const sensor_msgs::msg::LaserScan::SharedPtr msg)
{
//    RCLCPP_INFO(node_lidar_cam->get_logger(), "Received LaserScan message:");
//    RCLCPP_INFO(node_lidar_cam->get_logger(), "Header frame_id: %s", msg->header.frame_id.c_str());
//    RCLCPP_INFO(node_lidar_cam->get_logger(), "Angle min: %f, max: %f, increment: %f", msg->angle_min, msg->angle_max, msg->angle_increment);

    // Example of accessing the range data (distance to the first obstacle)
    if (!msg->ranges.empty()) {
 //       RCLCPP_INFO(node_lidar_cam->get_logger(), "First range value: %f meters", msg->ranges[0]);
    }
    float scanData[4];
    scanData[0] = msg->ranges[0];
    scanData[1] = msg->ranges[90];
    scanData[2] = msg->ranges[180];
    scanData[3] = msg->ranges[270];
    //  qDebug() << "scanData[0] : " << scanData[0];
    emit ldsReceiveSig(scanData);

    // You can iterate through msg->ranges for all the scan data
}
//export LD_LIBRARY_PATH=/opt/ros/humble/lib:$LD_LIBRARY_PATH
 //colcon build --symlink-install --packages-select kccistc_ros2_qt --cmake-args -DOpenCV_DIR=/opt/ros/humble/share/opencv4

void RosNodeLidarCam::image_callback(const sensor_msgs::msg::Image::SharedPtr msg) const
{
//    RCLCPP_INFO(node_lidar_cam->get_logger(), "Received image_callback message:");

    cv::Mat frame;
    try
    {
       frame = cv_bridge::toCvShare(msg, "bgr8")->image;
       imwrite("cap.jpg",frame);
    }
    catch (cv_bridge::Exception& e)
    {
        RCLCPP_ERROR(node_lidar_cam->get_logger(), "Could not convert image: %s", e.what());
    }

    cvtColor(frame, frame,  cv::COLOR_BGR2RGB);
    cv::line(frame, cv::Point((frame.cols >> 1) - 20, frame.rows >> 1), cv::Point((frame.cols >> 1) + 20, frame.rows >> 1), cv::Scalar(255, 0, 0), 3);
    cv::line(frame, cv::Point(frame.cols >> 1, (frame.rows >> 1) - 20), cv::Point(frame.cols >> 1, (frame.rows >> 1) + 20), cv::Scalar(255, 0, 0), 3);
    QImage * pImage = new QImage(frame.data, frame.cols, frame.rows, QImage::Format_RGB888);
    QImage repImage = pImage->scaled( pLcamView->height(),pLcamView->width(), Qt::KeepAspectRatio);

    pLcamView->setPixmap(QPixmap::fromImage(repImage));

}
rclcpp::Node::SharedPtr RosNodeLidarCam::getNode()
{
    return node_lidar_cam;
}
