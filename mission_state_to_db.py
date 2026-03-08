#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
import mysql.connector

class MissionStateToDB(Node):
    def __init__(self):
        super().__init__('mission_state_to_db')

        # ✅ 필요하면 파라미터로 빼도 됨(일단 고정값)
        self.db = mysql.connector.connect(
            host="127.0.0.1",        # DB가 다른 PC면 그 IP로
            user="hotel",
            password="hotel1234",
            database="hotel_db",
            autocommit=True
        )
        self.cur = self.db.cursor()

        self.sub = self.create_subscription(
            String, "/mission_state", self.cb, 10
        )
        self.get_logger().info("Subscribing /mission_state -> insert into robot_log")

#    def cb(self, msg: String):
#        # data 예: "이동중|로비|JOB-001|nav 시작"
#        raw = msg.data.strip()
#        parts = raw.split("|")
#
#        state = parts[0].strip() if len(parts) >= 1 and parts[0].strip() else "UNKNOWN"
#        location = parts[1].strip() if len(parts) >= 2 and parts[1].strip() else "UNKNOWN"
#        job_id = parts[2].strip() if len(parts) >= 3 and parts[2].strip() else None
#        message = "|".join(parts[3:]).strip() if len(parts) >= 4 else None
#
#        sql = """
#        INSERT INTO robot_log (state, location, job_id, message)
#        VALUES (%s, %s, %s, %s)
#        """
#        try:
#            self.cur.execute(sql, (state, location, job_id, message))
#            self.get_logger().info(f"DB INSERT OK: {state}, {location}, {job_id}, {message}")
#        except Exception as e:
#            self.get_logger().error(f"DB INSERT FAIL: {e} / raw='{raw}'")
    def cb(self, msg: String):
      raw = msg.data.strip()
    
      # 기본값
      state = "대기"
      location = "UNKNOWN"
      job_id = None
      message = None
    
      # 1) 에러/실패 문자열은 우선 처리 (기존과 동일하게 '원문 유지' 전략)
      raw_lower = raw.lower()
      if ("err" in raw_lower) or ("error" in raw_lower) or ("fail" in raw_lower):
          # 가능한 경우 job/room 추출은 해두고, message는 원문으로
          kv = {}
          for token in raw.split():
              if "=" in token:
                  k, v = token.split("=", 1)
                  kv[k.strip()] = v.strip()
    
          job_id = kv.get("job", None)
          location = kv.get("STEP", "UNKNOWN")
          message = raw  # ✅ 에러는 원문 유지
    
          # state는 ERROR로 통일
          state = "ERROR"
    
      else:
          # 2) 정상 메시지: "STEP=... job=... room=..." 파싱
          kv = {}
          for token in raw.split():
              if "=" in token:
                  k, v = token.split("=", 1)
                  kv[k.strip()] = v.strip()
    
          step = kv.get("STEP", "UNKNOWN")      # NAV_TO_ARM 등
          job_id = kv.get("job", None)          # job_...
          room = kv.get("room", None)           # ROOM_201
    
          # ✅ 요구사항 매핑
          # location <- STEP 값
          location = step
          # message <- room 값
          message = room
    
          # state <- '도착/이동중/대기' 로 변환
          # (너 프로젝트 기준으로 단계명을 상태로 묶는 룰)
          step_upper = step.upper()
    
          # 이동중으로 분류할 STEP 키워드들 (필요시 추가)
          moving_keys = ["NAV", "GO", "MOVE", "DRIVE", "TO_"]
          # 도착으로 분류할 키워드들 (필요시 추가)
          arrived_keys = ["ARRIVE", "ARRIVED", "REACHED", "DONE", "COMPLETE"]
          # 대기로 분류할 키워드들 (필요시 추가)
          waiting_keys = ["IDLE", "WAIT", "WAITING", "HOLD", "STANDBY", "BOOT"]
    
          if any(k in step_upper for k in arrived_keys):
              state = "도착"
          elif any(k in step_upper for k in moving_keys):
              state = "이동중"
          elif any(k in step_upper for k in waiting_keys):
              state = "대기"
          else:
              # 애매하면 기본 '대기'로 두거나, '작업중' 같은 상태를 새로 둬도 됨
              state = "대기"
    
      sql = """
      INSERT INTO robot_log (state, location, job_id, message)
      VALUES (%s, %s, %s, %s)
      """
      try:
          self.cur.execute(sql, (state, location, job_id, message))
          self.get_logger().info(f"DB INSERT OK: state={state}, loc={location}, job={job_id}, msg={message}")
      except Exception as e:
          self.get_logger().error(f"DB INSERT FAIL: {e} / raw='{raw}'")


def main():
    rclpy.init()
    node = MissionStateToDB()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == "__main__":
    main()
