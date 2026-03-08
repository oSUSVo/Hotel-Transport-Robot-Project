import socket
import subprocess

# 모든 IP에서 접속을 허용하려면 0.0.0.0으로 설정합니다.
HOST = '0.0.0.0' 
PORT = 5006

def main():
    print(f"📡 TCP 서버 시작... 포트 {PORT}에서 명령 대기 중")

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen()
        
        while True:
            # 클라이언트 접속 대기
            conn, addr = s.accept()
            with conn:
                print(f"\n[🤝 접속 완료] 클라이언트 IP: {addr}")
                data = conn.recv(1024)
                
                if not data:
                    continue
                
                # 받은 메시지 디코딩 (예: "{room_id: ROOM_201}")
                msg = data.decode('utf-8').strip()
                print(f"[📥 수신된 명령] {msg}")
                
                try:
                    print(f"▶️ ROS 2 로봇 미션 스크립트 실행 중... (명령어: python3 hotel_final.py '{msg}')")
                    
                    # subprocess.run은 hotel_final.py가 끝날 때까지 여기서 코드 실행을 멈추고 기다려줍니다.
                    result = subprocess.run(['python3', 'hotel_tcp.py', msg], capture_output=True, text=True)
                    
                    # returncode가 0이면 에러 없이 정상 종료됨을 의미합니다.
                    if result.returncode == 0:
                        print("[✅ 미션 성공] 클라이언트에게 'DONE' 응답 전송.")
                        conn.sendall("DONE".encode('utf-8'))
                    else:
                        print(f"[❌ 실행 에러 발생]\n{result.stderr}")
                        conn.sendall("ERROR".encode('utf-8'))
                        
                except Exception as e:
                    print(f"[💥 서버 내부 에러] {e}")
                    conn.sendall("ERROR".encode('utf-8'))

if __name__ == '__main__':
    main()
