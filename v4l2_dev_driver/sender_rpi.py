import cv2
import socket
import struct
import time

# ========= Configuration =========
RECEIVER_IP = "192.168.12.211"   # Receiver Pi's IP
PORT = 5001
FRAME_SKIP = 10
JPEG_QUALITY = 70  # compression level

# ========= Socket Setup =========
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((RECEIVER_IP, PORT))
print("[INFO] Connected to Receiver")

cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
encode_param = [int(cv2.IMWRITE_JPEG_QUALITY), JPEG_QUALITY]

frame_count = 0

try:
    while True:
        ret, frame = cap.read()
        if not ret:
            continue

        frame_count += 1
        if frame_count % FRAME_SKIP != 0:
            continue

        # Optional preprocessing: grayscale (you can modify)
        frame = cv2.resize(frame, (640, 480))
        # frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

        result, encimg = cv2.imencode('.jpg', frame, encode_param)
        data = encimg.tobytes()
        size = len(data)

        # Send size first, then data
        sock.sendall(struct.pack(">L", size) + data)

        time.sleep(0.05)

except KeyboardInterrupt:
    print("\n[INFO] Streaming stopped.")

finally:
    cap.release()
    sock.close()
~