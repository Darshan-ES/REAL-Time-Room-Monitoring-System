import os
import socket
import struct
import time
from glob import glob
import shutil
import subprocess

HOST = '192.168.12.211'
PORT = 5001
FRAME_DIR = './frames'
SENT_DIR = './sent'

os.makedirs(SENT_DIR, exist_ok=True)

# Launch capture program
if not os.path.isfile('./simple_capture'):
    raise FileNotFoundError("Missing ./simple_capture binary")

proc = subprocess.Popen(['./simple_capture'], stdout=subprocess.DEVNULL)

# Connect socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    sock.connect((HOST, PORT))
except socket.error as e:
    print(f"[ERROR] Could not connect to {HOST}:{PORT} -{e}")
    proc.terminate()
    exit(1)

print(f"[INFO] Connected to {HOST}:{PORT}")

def send_all(sock, data):
    total_sent = 0
    while total_sent < len(data):
        sent = sock.send(data[total_sent:])
        if sent == 0:
            raise RuntimeError("Socket connection broken")
        total_sent += sent

try:
    while True:
        frame_files = sorted(glob(os.path.join(FRAME_DIR, '*.jpg')))
        if not frame_files:
            time.sleep(0.1)
            continue

        for filepath in frame_files:
            if not os.path.isfile(filepath):
                continue
            with open(filepath, 'rb') as f:
                data = f.read()

            header = struct.pack(">L", len(data))
            send_all(sock, header + data)

            print(f"[SENT] {os.path.basename(filepath)} ({len(data)} bytes)")
            shutil.move(filepath, os.path.join(SENT_DIR, os.path.basename(filepath)))

except KeyboardInterrupt:
    print("[INFO] Sender interrupted by user")

except Exception as e:
    print(f"[ERROR] {type(e).__name__}: {e}")

finally:
    sock.close()
    print("[INFO] Socket closed cleanly")

    if proc.poll() is None:
        proc.terminate()
        proc.wait()
        print("[INFO] simple_capture terminated")
