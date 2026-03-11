import rclpy
from rclpy.node import Node
from rclpy.action import ActionClient
from trajectory_msgs.msg import JointTrajectory, JointTrajectoryPoint
from control_msgs.action import GripperCommand
from builtin_interfaces.msg import Duration
from std_msgs.msg import String
import time
import threading # 로봇이 움직이는 동안에도 ROS2 통신이 끊기지 않도록 도와주는 라이브러리

class HotelRobotWorker(Node):
    def __init__(self):
        # 1. 노드 이름 설정
        super().__init__('hotel_robot_worker')
        
        # 2. 로봇 팔 제어용 Publisher 설정 (Topic 방식)
        # /arm_controller/joint_trajectory 토픽으로 궤적 데이터를 쏴서 팔을 움직입니다.
        self.arm_pub = self.create_publisher(JointTrajectory, '/arm_controller/joint_trajectory', 10)
        
        # 3. 그리퍼 제어용 Action Client 설정 (Action 방식)
        # 단순히 값을 던지는 게 아니라, '목표치까지 도달했는지' 확인하기 위해 Action을 씁니다.
        self.gripper_client = ActionClient(self, GripperCommand, '/gripper_controller/gripper_cmd')
        
        # 4. 호텔 방 번호 호출을 기다리는 Subscriber
        # 터미널이나 다른 노드에서 /hotel/room_number 토픽으로 방 번호를 보내면 room_callback 함수가 실행됩니다.
        self.sub = self.create_subscription(String, '/hotel/room_number', self.room_callback, 10)

        # 5. 로봇 팔의 주요 위치 좌표 (단위: Radian)
        self.home_pose = [0.0, -1.57, 1.58, 1.57, 0.0] # 초기 대기 자세 (안전한 위치)
        self.zero_pose = [0.0, 0.0, 0.0, 0.0, 0.0]     # 모든 관절이 0인 상태 (로봇이 'ㄱ'자로 서는 자세)
        
        # 터틀봇(TurtleBot) 적재함 관련 좌표
        self.turtlebot_above = [1.805, 0.482, -0.673, 1.644, -0.114] # 짐을 놓기 전, 충돌 방지를 위해 트렁크 위에서 대기하는 위치
        self.turtlebot_place = [1.821, 0.580, -0.336, 1.230, -0.117] # 실제로 짐을 내려놓기 위해 하강한 위치
        
        # 호텔 호실별 짐이 놓인 좌표 (직접 티칭해서 얻은 정밀 좌표)
        self.room_poses = {
            '201': [0.316, 1.002, -1.252, 1.583, 0.005],
            '202': [-0.219, 0.830, -0.937, 1.462, -0.143],
            '203': [-0.647, 0.939, -1.124, 1.566, -0.072]
        }
        
        self.get_logger().info('>>> [준비 완료] 방 번호(예: 201)를 보내주세요! <<<')
        
        # 6. 로봇 중복 동작 방지용 안전 스위치
        # 로봇이 움직이는 도중에 다른 방 번호가 입력되어 꼬이는 것을 막아줍니다.
        self.is_working = False

    def room_callback(self, msg):
        """
        Subscriber가 메시지를 받으면 자동으로 실행되는 콜백 함수입니다.
        """
        room_num = msg.data # 수신된 메시지에서 문자열(방 번호)만 추출
        
        # 이미 로봇이 움직이고 있다면 새로운 명령은 무시 (안전장치)
        if self.is_working:
            self.get_logger().warn('⚠️ 현재 로봇이 작업 중입니다. 대기해주세요.')
            return
            
        # 입력된 방 번호가 우리가 아는 번호인지 확인
        if room_num in self.room_poses:
            # 💡 중요: 콜백 함수 안에서 time.sleep()이 길어지면 ROS2 통신 자체가 멈춥니다.
            # 이를 방지하기 위해 로봇이 움직이는 작업(execute_mission_sequence)은 
            # 별도의 스레드(Thread)로 빼서 병렬로 실행시킵니다.
            threading.Thread(target=self.execute_mission_sequence, args=(room_num,)).start()
        else:
            self.get_logger().error(f'❌ 등록되지 않은 방 번호입니다: {room_num}')

    def move_arm(self, positions, sec=2):
        """
        로봇 팔의 5개 관절을 지정된 좌표로 이동시키는 함수
        :param positions: 이동할 관절 각도 리스트 (5개)
        :param sec: 이동에 걸리는 시간 (기본값 2초)
        """
        msg = JointTrajectory()
        # 제어할 조인트 이름 (ROBOTIS OMX 6축 매핑 기준)
        msg.joint_names = ['joint1', 'joint2', 'joint3', 'joint4', 'joint5']
        
        point = JointTrajectoryPoint()
        point.positions = positions # 목표 각도 삽입
        point.time_from_start = Duration(sec=sec, nanosec=0) # 이동 시간 설정
        
        msg.points.append(point)
        self.arm_pub.publish(msg) # 컨트롤러로 명령 발송
        
        # 💡 물리적인 로봇 팔이 목표 위치에 완전히 도착할 때까지 코드 진행을 멈추고 기다립니다.
        # 여유 시간(0.5초)을 주어 동작이 부드럽게 이어지도록 합니다.
        time.sleep(sec + 0.5)

    def control_gripper(self, position, effort=1.0):
        """
        그리퍼(집게)를 열거나 닫는 함수
        :param position: 1.0(완전히 엶) ~ 0.0 또는 음수(꽉 닫음)
        :param effort: 잡는 힘 (기본값 1.0, 최대치)
        """
        # 그리퍼 제어 서버가 켜져 있는지 1초 동안 확인
        if not self.gripper_client.wait_for_server(timeout_sec=1.0):
            self.get_logger().error('그리퍼 서버 연결 실패! (컨트롤러가 켜져있는지 확인하세요)')
            return
            
        goal_msg = GripperCommand.Goal()
        goal_msg.command.position = position
        goal_msg.command.max_effort = effort
        
        # 목표값을 서버로 비동기(순서 안 기다림) 전송
        self.gripper_client.send_goal_async(goal_msg)

    def execute_mission_sequence(self, room_num):
        """
        호텔 로봇의 전체 Pick & Place(집고 놓기) 시나리오를 순서대로 실행하는 함수
        이 과정이 실행되는 동안에는 is_working이 True가 되어 다른 명령을 받지 않습니다.
        """
        self.is_working = True
        target_pose = self.room_poses[room_num]
        
        self.get_logger().info(f'\n========== 📦 {room_num}호 미션 시작! ==========')

        # --- [1부: 짐 집어 올리기 (Pick)] ---
        self.get_logger().info('1. 기본 자세로 이동')
        self.move_arm(self.home_pose)

        self.get_logger().info('2. 그리퍼 열기')
        self.control_gripper(1.0) # 활짝 열기
        time.sleep(1.0) # 그리퍼가 열릴 때까지 잠시 대기

        self.get_logger().info('3. 0점(ㄱ자) 자세 경유')
        self.move_arm(self.zero_pose) # 경로 꼬임 방지를 위한 중간 기착지

        self.get_logger().info(f'4. {room_num}호 짐 위치로 이동')
        self.move_arm(target_pose)

        self.get_logger().info('5. 그리퍼 닫기 (짐 꽉 잡기)')
        self.control_gripper(-0.1) # 마이너스 값을 주어 물건을 단단히 고정
        time.sleep(1.0)

        self.get_logger().info('6. 짐 들어올리기 (Joint 2 UP)')
        # 파이썬의 .copy()를 사용하여 원본 좌표는 건드리지 않고 복사본 생성
        lift_pose = target_pose.copy()
        lift_pose[1] = -0.8 # 두 번째 관절(어깨)만 위로 들어올리도록 각도 수정
        self.move_arm(lift_pose)

        # --- [2부: 터틀봇에 싣기 (Place)] ---
        self.get_logger().info('7. 터틀봇 트렁크 위로 이동')
        self.move_arm(self.turtlebot_above) # 충돌 방지를 위해 위쪽 공간 확보

        self.get_logger().info('8. 짐 싣기 위치로 내리기')
        self.move_arm(self.turtlebot_place) # 조심스럽게 하강

        self.get_logger().info('9. 그리퍼 살짝 열기 (0.7)')
        self.control_gripper(0.7) # 짐을 놓기 위해 그리퍼 열기
        time.sleep(1.0)

        self.get_logger().info('10. 다시 트렁크 위로 빠져나오기')
        self.move_arm(self.turtlebot_above) # 짐을 놓은 상태로 수직 상승하여 빠져나옴

        self.get_logger().info('11. 0점(ㄱ자) 자세로 복귀')
        self.move_arm(self.zero_pose) # 원상 복귀를 위한 중간 기착지

        self.get_logger().info('12. 최종 대기 자세로 이동')
        self.move_arm(self.home_pose)
        
        self.get_logger().info(f'========== ✅ {room_num}호 미션 완벽 종료! ==========\n')
        
        # 모든 작업이 끝났으므로 다른 방 번호를 받을 수 있도록 스위치를 끕니다.
        self.is_working = False


def main():
    # ROS 2 초기화
    rclpy.init()
    
    # 우리가 만든 클래스 노드를 생성
    worker = HotelRobotWorker()
    
    try:
        # spin() 함수는 프로그램이 종료되지 않고 
        # 토픽(방 번호)이 들어올 때까지 계속 대기하며 귀를 기울이게 만듭니다.
        rclpy.spin(worker)
    except KeyboardInterrupt:
        # 사용자가 Ctrl+C를 누르면 안전하게 종료되도록 예외 처리
        pass
    finally:
        # 사용이 끝난 노드를 파괴하고 ROS 2를 정상 종료합니다.
        worker.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    # 이 파이썬 파일이 직접 실행되었을 때만 main() 함수를 호출합니다.
    main()