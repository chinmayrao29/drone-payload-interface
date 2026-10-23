import serial
import json
import time

# Serial connection to STM32 on module PCB
MODULE_PORT = '/dev/ttyUSB1'
MODULE_BAUD = 115200

def main():
    print("LiDAR module starting...")

    while True:
        try:
            port = serial.Serial(MODULE_PORT, MODULE_BAUD, timeout=1)
            print(f"Connected to LiDAR module on {MODULE_PORT}")
            break
        except serial.SerialException:
            print(f"Waiting for LiDAR module on {MODULE_PORT}...")
            time.sleep(2)

    while True:
        try:
            if port.in_waiting > 0:
                line = port.readline().decode('utf-8', errors='ignore')
                if line.strip():
                    try:
                        data = json.loads(line.strip())
                        print(f"LiDAR data: {data}")
                    except json.JSONDecodeError:
                        print(f"Raw: {line.strip()}")

            time.sleep(0.1)

        except KeyboardInterrupt:
            print("LiDAR module stopping...")
            port.close()
            break
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(1)

if __name__ == "__main__":
    main()
