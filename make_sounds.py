# make_sounds.py
import os
from gtts import gTTS

save_dir = "sounds"
if not os.path.exists(save_dir):
    os.makedirs(save_dir)

texts = {
    "elevator_ready.mp3": "The elevator is ready",
    "close_door.mp3": "The door is closing.",
    "open_door.mp3": "The door is opening.",
    "1st_floor.mp3": "First floor.",
    "2nd_floor.mp3": "Second floor."
}

for filename, text in texts.items():
    print(f"Creating '{filename}'...")
    tts = gTTS(text=text, lang='en')
    tts.save(os.path.join(save_dir, filename))

print("All sound files created!")
