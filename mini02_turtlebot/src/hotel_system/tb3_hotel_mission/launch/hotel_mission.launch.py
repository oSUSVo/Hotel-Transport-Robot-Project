from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():
    locations = PathJoinSubstitution([FindPackageShare("tb3_hotel_mission"), "config", "locations.yaml"])

    return LaunchDescription([
        Node(
            package="tb3_hotel_mission",
            executable="tb3_hotel_mission_node",
            output="screen",
            parameters=[locations],
        ),
    ])