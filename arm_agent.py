#!/usr/bin/env python3
import json
import socket
import subprocess
import threading

HOST = "0.0.0.0"
PORT = 5005

# arm_final.py 절대경로로 바꿔주세요
ARM_SCRIPT = "/root/ros2_ws/src/hotel_service/hotel_service/arm_final.py"   # 예시
PYTHON = "python3"

def handle_client(conn, addr):
    try:
        buf = b""
        while True:
            chunk = conn.recv(4096)
            if not chunk:
                break
            buf += chunk
            if b"\n" in buf:
                line, _, rest = buf.partition(b"\n")
                buf = rest

                req = json.loads(line.decode("utf-8").strip())
                # 기대 포맷: {"job_id":"...", "room_id":"ROOM_201"}
                room_id = req.get("room_id", "")
                job_id = req.get("job_id", "")

                if not room_id.startswith("ROOM_"):
                    resp = {"ok": False, "job_id": job_id, "error": f"bad room_id: {room_id}"}
                    conn.sendall((json.dumps(resp) + "\n").encode("utf-8"))
                    continue

                # arm_final.py가 원하는 인자 형식 그대로 전달
                arg = f'{{room_id: {room_id}}}'

                # 실행 (동기 실행: 완료될 때까지 대기)
                p = subprocess.run(
                    [PYTHON, ARM_SCRIPT, arg],
                    capture_output=True,
                    text=True
                )

                resp = {
                    "ok": (p.returncode == 0),
                    "job_id": job_id,
                    "room_id": room_id,
                    "returncode": p.returncode,
                    "stdout": p.stdout[-2000:],  # 너무 길면 잘라서
                    "stderr": p.stderr[-2000:],
                }
                conn.sendall((json.dumps(resp) + "\n").encode("utf-8"))
    except Exception as e:
        try:
            conn.sendall((json.dumps({"ok": False, "error": str(e)}) + "\n").encode("utf-8"))
        except Exception:
            pass
    finally:
        conn.close()

def main():
    print(f"[arm_agent] listening on {HOST}:{PORT}")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen(20)
        while True:
            conn, addr = s.accept()
            threading.Thread(target=handle_client, args=(conn, addr), daemon=True).start()

if __name__ == "__main__":
    main()
