#!/usr/bin/env python3
import json
import socket
import time
import os
import pygame
import subprocess
from typing import Dict, Any

from RPLCD.i2c import CharLCD

# 설정
HOST = "0.0.0.0"
PORT = 5005
I2C_ADDR = 0x27
I2C_PORT = 1
LCD_COLS = 16
LCD_ROWS = 2

# 오디오 파일 경로
AUDIO_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sounds")

def set_system_volume(volume_str="100%"):
    try:
        subprocess.run(["pactl", "set-sink-volume", "@DEFAULT_SINK@", volume_str], check=True)
    except Exception:
        pass

def init_audio():
    try:
        pygame.mixer.pre_init(44100, -16, 2, 1024)
        pygame.mixer.init()
        pygame.mixer.music.set_volume(1.0)
    except Exception as e:
        print(f"Audio Init Error: {e}")

def play_sound(filename: str, wait=True):
    try:
        file_path = os.path.join(AUDIO_DIR, filename)
        if os.path.exists(file_path):
            pygame.mixer.music.load(file_path)
            pygame.mixer.music.play()
            if wait:
                while pygame.mixer.music.get_busy():
                    time.sleep(0.1)
    except Exception as e:
        print(f"Play error: {e}")

def lcd_print(lcd: CharLCD, line1: str, line2: str = ""):
    lcd.clear()
    lcd.cursor_pos = (0, 0)
    lcd.write_string(line1[:LCD_COLS].ljust(LCD_COLS))
    lcd.cursor_pos = (1, 0)
    lcd.write_string(line2[:LCD_COLS].ljust(LCD_COLS))

def handle(req: Dict[str, Any], lcd: CharLCD) -> Dict[str, Any]:
    job_id = str(req.get("job_id", ""))
    command = str(req.get("command", ""))
    from_floor = int(req.get("from_floor", 1))
    to_floor = int(req.get("to_floor", 1))

    if command == "CALL":
        # 터틀봇이 엘리베이터를 불렀을 때 (문 열림)
        lcd_print(lcd, f"Floor {from_floor}", "OPEN DOOR")
        play_sound("open_door.mp3", wait=True)
        return {"job_id": job_id, "success": True, "message": f"CALL ok: {from_floor}F"}

    if command == "GO":
        # 1. 탑승 직후 (문이 열려있는 상태 유지 - 소리 없이 LCD만)
        lcd_print(lcd, f"Floor {from_floor}", "OPEN DOOR")
        time.sleep(2)  # 로봇이 들어갈 시간 대기

        # 2. 문 닫힘 및 이동 시작
        lcd_print(lcd, "CLOSE DOOR", f"Going to {to_floor}F")
        play_sound("close_door.mp3", wait=True)

        # 3. 이동 중 연출 (3초)
        time.sleep(3)

        # 4. 도착 층 안내 (1st Floor / 2nd Floor)
        floor_text = "1st Floor" if to_floor == 1 else "2nd Floor"
        lcd_print(lcd, floor_text, "Arrived")

        # 층 안내 음성
        sound_file = "1st_floor.mp3" if to_floor == 1 else "2nd_floor.mp3"
        play_sound(sound_file, wait=True)

        # 5. 최종 문 열림 (로봇 하차 시작)
        lcd_print(lcd, floor_text, "OPEN DOOR")
        
        # 여기서 wait=False로 설정하여 소리 재생과 동시에 응답을 보낼 준비를 합니다.
        play_sound("open_door.mp3", wait=False)

        # ★ 즉각 응답 생성: 이 메시지가 전송되면 터틀봇은 바로 주행을 시작합니다.
        response_data = {"job_id": job_id, "success": True, "message": f"Arrived {to_floor}F"}

        # 6. 하차 뒷정리 (터틀봇은 이미 주행 중)
        time.sleep(2)  # 로봇이 엘리베이터에서 나가는 시간 확보
        lcd_print(lcd, floor_text, "")
        time.sleep(2)  # CLOSING 문구 유지

        return response_data

    return {"job_id": job_id, "success": False, "message": "Unknown command"}

def main():
    set_system_volume("100%")
    init_audio()
    lcd = CharLCD('PCF8574', I2C_ADDR, port=I2C_PORT, cols=LCD_COLS, rows=LCD_ROWS)
    lcd_print(lcd, "Elevator System", "Online")

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen(5)
        print(f"Server started on {HOST}:{PORT}")
        while True:
            conn, addr = s.accept()
            with conn:
                try:
                    data = conn.recv(4096).decode("utf-8").strip()
                    if not data: continue
                    req = json.loads(data)
                    
                    # handle 함수가 작업을 완료하고 최종 response를 반환함
                    resp = handle(req, lcd)
                    
                    # 터틀봇에게 결과 전송
                    conn.sendall((json.dumps(resp) + "\n").encode("utf-8"))
                except Exception as e:
                    print(f"Error: {e}")

if __name__ == "__main__":
    main()
