import serial
import time
import re
import json

MODULE_PORT = '/dev/ttyUSB0'
MODULE_BAUD = 115200

TETHER_STATUS_FILE = '/tmp/tether_status.txt'
TETHER_COMMAND_FILE = '/tmp/tether_command.txt'

def parse_tether_status(line):
    match = re.match(
        r'TETHER_STATUS:depth=([\d.]+):gas_ppm=([\d.]+):temp=([\d.]+):moving=(\w+)',
        line
    )
    if match:
        depth, gas, temp, moving = match.groups()
        return {
            "depth": float(depth),
            "gas_ppm": float(gas),
            "temp": float(temp),
            "moving": moving == "true"
        }
    return None

def check_pending_command(port):
    try:
        import os
        if os.path.exists(TETHER_COMMAND_FILE):
            with open(TETHER_COMMAND_FILE, 'r') as f:
                command = f.read().strip()
            os.remove(TETHER_COMMAND_FILE)
            if command:
                port.write((command + '\n').encode())
                print(f"Sent command: {command}")
    except Exception as e:
        print(f"Command error: {e}")

def main():
    print("Tether module handler starting...")

    while True:
        try:
            port = serial.Serial(MODULE_PORT, MODULE_BAUD, timeout=1)
            print(f"Connected to tether module on {MODULE_PORT}")
            break
        except serial.SerialException:
            print(f"Waiting for tether module on {MODULE_PORT}...")
            time.sleep(2)

    while True:
        try:
            check_pending_command(port)

            if port.in_waiting > 0:
                line = port.readline().decode('utf-8', errors='ignore').strip()
                if line.startswith("TETHER_STATUS"):
                    status = parse_tether_status(line)
                    if status:
                        print(f"Tether status: {status}")
                        with open(TETHER_STATUS_FILE, 'w') as f:
                            f.write(json.dumps(status))

            time.sleep(0.1)

        except KeyboardInterrupt:
            port.close()
            break
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(1)

if __name__ == "__main__":
    main()
