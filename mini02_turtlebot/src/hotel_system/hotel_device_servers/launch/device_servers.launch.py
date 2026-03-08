from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(package="hotel_device_servers", executable="arm_server", output="screen"),
        Node(package="hotel_device_servers", executable="elevator_server", output="screen"),
        Node(package="hotel_device_servers", executable="room_server", output="screen"),
    ])