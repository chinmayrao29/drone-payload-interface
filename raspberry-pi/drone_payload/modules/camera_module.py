import serial
import time
import re

MODULE_PORT = '/dev/ttyUSB0'  # same port payload_manager uses
MODULE_BAUD = 115200

def parse_camera_ready(line):
    # CAMERA_READY:ip=192.168.72.45:port=80:path=/stream
    match = re.match(r'CAMERA_READY:ip=([\d.]+):port=(\d+):path=(\S+)', line)
    if match:
        ip, port, path = match.groups()
        return f"http://{ip}:{port}{path}"
    return None

def main():
    print("Camera module handler starting...")
    
    while True:
        try:
            port = serial.Serial(MODULE_PORT, MODULE_BAUD, timeout=1)
            break
        except serial.SerialException:
            time.sleep(2)

    stream_url = None
    while True:
        try:
            if port.in_waiting > 0:
                line = port.readline().decode('utf-8', errors='ignore').strip()
                if line.startswith("CAMERA_READY"):
                    stream_url = parse_camera_ready(line)
                    print(f"Camera stream available at: {stream_url}")
                    # write to a shared file or socket for dashboard_server to read
            time.sleep(0.1)
        except KeyboardInterrupt:
            port.close()
            break

if __name__ == "__main__":
    main()
