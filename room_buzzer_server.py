#!/usr/bin/env python3
import json
import socket
import time
from typing import Dict, Any
import RPi.GPIO as GPIO

HOST = "0.0.0.0"
PORT = 5005

# 객실별 부저가 연결된 라즈베리파이 BCM 핀 번호 매핑
ROOM_BUZZER_MAP = {
    201: 17,
    202: 22,
    203: 27
}

# 객실별 LED가 연결된 라즈베리파이 BCM 핀 번호 매핑 (새로 추가)
ROOM_LED_MAP = {
    201: 18,
    202: 23,
    203: 24
}

#def setup_gpio():
#    """모든 부저 및 LED의 GPIO 핀 초기 설정"""
#    GPIO.setwarnings(False)
#    GPIO.setmode(GPIO.BCM)
#    
#    # 부저 핀 초기화
#    for room, pin in ROOM_BUZZER_MAP.items():
#        GPIO.setup(pin, GPIO.OUT)
#        GPIO.output(pin, GPIO.LOW)
#        
#    # LED 핀 초기화
#    for room, pin in ROOM_LED_MAP.items():
#        GPIO.setup(pin, GPIO.OUT)
#        GPIO.output(pin, GPIO.LOW)

def setup_gpio():
    GPIO.setwarnings(False)
    GPIO.setmode(GPIO.BCM)

    # 부저 핀 초기화 (initial 명시!)
    for room, pin in ROOM_BUZZER_MAP.items():
      GPIO.setup(pin, GPIO.OUT, initial=GPIO.LOW)

    # LED 핀 초기화 (initial 명시!)
    for room, pin in ROOM_LED_MAP.items():
      GPIO.setup(pin, GPIO.OUT, initial=GPIO.LOW)

def play_buzzer(buzzer_pin: int, led_pin: int, play_count: int = 2):
    """
    부저에 맞춘 '고급 호텔 딩동' (쇳소리 줄인 저~중역대) + LED 제어
    """
    # 부저가 울리기 시작할 때 LED 켜기
    GPIO.output(led_pin, GPIO.HIGH)
    
    pwm = GPIO.PWM(buzzer_pin, 660)
    pwm.start(0)

    def chime(base_freq: int, dur: float,
              duty_peak: float = 24.0,
              attack: float = 0.06,
              release: float = 0.60,
              mix_octave: bool = False,     
              octave_ratio: float = 1.5,    
              slice_ms: float = 4.0):
        
        attack = max(0.01, min(attack, dur * 0.35))
        release = max(0.08, min(release, dur * 0.95))
        sustain = max(0.0, dur - attack - release)

        dt = slice_ms / 1000.0
        t = 0.0
        toggle = False

        def duty_at(ts: float) -> float:
            if ts < attack:
                x = ts / attack
                return duty_peak * (x * x)  
            if ts < attack + sustain:
                return duty_peak
            x = (ts - attack - sustain) / max(1e-6, release)
            return max(0.0, duty_peak * (1.0 - x) ** 2.4)  

        while t < dur:
            duty = duty_at(t)
            pwm.ChangeDutyCycle(duty)

            if mix_octave and duty > 1.0:
                toggle = not toggle
                f = int(base_freq * (octave_ratio if toggle else 1.0))
                pwm.ChangeFrequency(max(50, f))
            else:
                pwm.ChangeFrequency(base_freq)

            time.sleep(dt)
            t += dt

        pwm.ChangeDutyCycle(0)

    try:
        for _ in range(play_count):
            # 딩: 중역(부드럽고 고급스러운 톤)
            chime(base_freq=660, dur=0.55, duty_peak=22, attack=0.07, release=0.40,
                  mix_octave=False, slice_ms=4.0)

            time.sleep(0.10)

            # 동: 더 낮고 더 길게(잔향 강조)
            chime(base_freq=520, dur=0.85, duty_peak=20, attack=0.08, release=0.70,
                  mix_octave=False, slice_ms=4.0)

            time.sleep(0.45)

    finally:
        pwm.ChangeDutyCycle(0)
        pwm.stop()
        
        # 부저가 완전히 끝나면 LED 끄기
        GPIO.output(led_pin, GPIO.LOW)

def handle(req: Dict[str, Any]) -> Dict[str, Any]:
    job_id = str(req.get("job_id", ""))
    command = str(req.get("command", ""))
    room_id = int(req.get("room_id", 0))

    if command != "ARRIVE":
        return {"job_id": job_id, "success": False, "message": f"Unknown command: {command}"}

    if room_id in ROOM_BUZZER_MAP and room_id in ROOM_LED_MAP:
        buzzer_pin = ROOM_BUZZER_MAP[room_id]
        led_pin = ROOM_LED_MAP[room_id]
        print(f"[{room_id}호] 로봇 도착. 부저(BCM {buzzer_pin})와 LED(BCM {led_pin}) 작동 시작.")
        
        play_buzzer(buzzer_pin, led_pin, play_count=2)  
        return {"job_id": job_id, "success": True, "message": f"Arrived at Room {room_id}, Buzzer & LED OK"}
    else:
        return {"job_id": job_id, "success": False, "message": f"Invalid room_id: {room_id}"}

def recv_json_line(conn: socket.socket) -> Dict[str, Any]:
    data = b""
    while not data.endswith(b"\n"):
        chunk = conn.recv(4096)
        if not chunk:
            break
        data += chunk
    if not data:
        return {}
    return json.loads(data.decode("utf-8").strip())

def send_json_line(conn: socket.socket, obj: Dict[str, Any]) -> None:
    conn.sendall((json.dumps(obj) + "\n").encode("utf-8"))

#def main():
#    setup_gpio()
#    print(f"[raspi_multi_room_buzzer_agent] listen {HOST}:{PORT}")
#    
#    try:
#        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
#            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
#            s.bind((HOST, PORT))
#            s.listen(10)
#
#            while True:
#                conn, addr = s.accept()
#                with conn:
#                    try:
#                        req = recv_json_line(conn)
#                        resp = handle(req)
#                        send_json_line(conn, resp)
#                    except Exception as e:
#                        send_json_line(conn, {"success": False, "message": f"exception: {e}"})
#    finally:
#        GPIO.cleanup()
def main():
    setup_gpio()
    print(f"[raspi_multi_room_buzzer_agent] listen {HOST}:{PORT}")

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind((HOST, PORT))
            s.listen(10)

            try:
                while True:
                    conn, addr = s.accept()   # 여기서 Ctrl+C 누르면 KeyboardInterrupt
                    with conn:
                        try:
                            req = recv_json_line(conn)
                            resp = handle(req)
                            send_json_line(conn, resp)
                        except Exception as e:
                            send_json_line(conn, {"success": False, "message": f"exception: {e}"})

            except KeyboardInterrupt:
                print("\n[raspi_multi_room_buzzer_agent] stopped by Ctrl+C")

    finally:
        GPIO.cleanup()
        print("[raspi_multi_room_buzzer_agent] GPIO cleaned up")

if __name__ == "__main__":
    main()
