import sys
import re
import rclpy
from rclpy.node import Node
from rclpy.action import ActionClient
from trajectory_msgs.msg import JointTrajectory, JointTrajectoryPoint
from control_msgs.action import GripperCommand
from builtin_interfaces.msg import Duration
import time
import threading

class HotelRobotWorker(Node):
    def __init__(self):
        super().__init__('hotel_robot_worker')
        
        self.arm_pub = self.create_publisher(JointTrajectory, '/arm_controller/joint_trajectory', 10)
        self.gripper_client = ActionClient(self, GripperCommand, '/gripper_controller/gripper_cmd')
        
        self.home_pose = [0.0, -1.57, 1.58, 1.57, 0.0]
        self.zero_pose = [0.0, 0.0, 0.0, 0.0, 0.0] 
        
        self.turtlebot_above = [1.805, 0.482, -0.673, 1.644, -0.114]
        self.turtlebot_place = [1.821, 0.580, -0.336, 1.230, -0.117]
        
        self.room_poses = {
            '201': [0.316, 1.002, -1.252, 1.583, 0.005],
            '202': [-0.219, 0.830, -0.937, 1.462, -0.143],
            '203': [-0.647, 0.939, -1.124, 1.566, -0.072]
        }
        
        # 💡 중요: 단발성 실행 시, 노드가 켜지자마자 데이터를 쏘면 로봇이 못 받을 수 있습니다.
        # Publisher가 로봇과 통신을 연결할 수 있도록 1.5초 정도 숨을 고릅니다.
        self.get_logger().info('로봇 컨트롤러와 연결 중입니다...')
        time.sleep(1.5)

    def move_arm(self, positions, sec=2):
        msg = JointTrajectory()
        msg.joint_names = ['joint1', 'joint2', 'joint3', 'joint4', 'joint5']
        point = JointTrajectoryPoint()
        point.positions = positions
        point.time_from_start = Duration(sec=sec, nanosec=0)
        msg.points.append(point)
        self.arm_pub.publish(msg)
        time.sleep(sec + 0.5)

    def control_gripper(self, position, effort=1.0):
        if not self.gripper_client.wait_for_server(timeout_sec=1.0):
            self.get_logger().error('그리퍼 서버 연결 실패!')
            return
        goal_msg = GripperCommand.Goal()
        goal_msg.command.position = position
        goal_msg.command.max_effort = effort
        self.gripper_client.send_goal_async(goal_msg)

    def execute_mission_sequence(self, room_num):
        target_pose = self.room_poses[room_num]
        self.get_logger().info(f'\n========== 📦 {room_num}호 미션 시작! ==========')

        self.get_logger().info('1. 기본 자세로 이동')
        self.move_arm(self.home_pose)

        self.get_logger().info('2. 그리퍼 열기')
        self.control_gripper(1.0)
        time.sleep(1.0)

        self.get_logger().info('3. 0점(ㄱ자) 자세 경유')
        self.move_arm(self.zero_pose)

        self.get_logger().info(f'4. {room_num}호 짐 위치로 이동')
        self.move_arm(target_pose)

        self.get_logger().info('5. 그리퍼 닫기 (짐 꽉 잡기)')
        self.control_gripper(-0.1)
        time.sleep(1.0)

        self.get_logger().info('6. 짐 들어올리기 (Joint 2 UP)')
        lift_pose = target_pose.copy()
        lift_pose[1] = -0.8
        self.move_arm(lift_pose)

        self.get_logger().info('7. 터틀봇 트렁크 위로 이동')
        self.move_arm(self.turtlebot_above)

        self.get_logger().info('8. 짐 싣기 위치로 내리기')
        self.move_arm(self.turtlebot_place)

        self.get_logger().info('9. 그리퍼 살짝 열기 (0.7)')
        self.control_gripper(0.7)
        time.sleep(1.0)

        self.get_logger().info('10. 다시 트렁크 위로 빠져나오기')
        self.move_arm(self.turtlebot_above)

        self.get_logger().info('11. 0점(ㄱ자) 자세로 복귀')
        self.move_arm(self.zero_pose)

        self.get_logger().info('12. 최종 대기 자세로 이동')
        self.move_arm(self.home_pose)
        
        self.get_logger().info(f'========== ✅ {room_num}호 미션 완벽 종료! ==========\n')


def main():
    rclpy.init()

    # 1. 터미널에서 입력한 명령어 길이 확인
    if len(sys.argv) < 2:
        print('❌ 오류: 방 번호 인자가 없습니다.\n사용법: python3 hotel_final.py "{room_id: ROOM_201}"')
        rclpy.shutdown()
        return

    # 2. 정규표현식으로 입력값에서 숫자(방 번호)만 쏙 뽑아내기
    arg_str = sys.argv[1] # 예: "{room_id: ROOM_201}"
    match = re.search(r'ROOM_(\d+)', arg_str)
    
    if not match:
        print(f'❌ 오류: 인자 형식이 잘못되었습니다. (입력값: {arg_str})')
        rclpy.shutdown()
        return

    room_num = match.group(1) # "201" 추출 완료

    worker = HotelRobotWorker()

    if room_num in worker.room_poses:
        # 3. Action 통신이 끊기지 않게 시퀀스를 스레드로 실행하고, 메인은 spin_once로 계속 귀를 열어둡니다.
        mission_thread = threading.Thread(target=worker.execute_mission_sequence, args=(room_num,))
        mission_thread.start()

        # 시퀀스가 끝날 때까지 백그라운드 통신 유지
        while mission_thread.is_alive():
            rclpy.spin_once(worker, timeout_sec=0.1)
    else:
        worker.get_logger().error(f'❌ 등록되지 않은 방 번호입니다: {room_num}')

    # 4. 임무가 끝나면 깔끔하게 노드 파괴 후 스크립트 종료
    worker.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()