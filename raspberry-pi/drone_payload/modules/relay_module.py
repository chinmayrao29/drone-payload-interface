import serial
import time
import re

MODULE_PORT = '/dev/ttyUSB0'
MODULE_BAUD = 115200

RELAY_STATUS_FILE = '/tmp/relay_status.txt'

def parse_relay_status(line):
    match = re.match(
        r'RELAY_READY:freq=(\d+):nodes=(\d+):rssi_a=(-?\d+):rssi_b=(-?\d+)',
        line
    )
    if match:
        freq, nodes, rssi_a, rssi_b = match.groups()
        return {
            "frequency": int(freq),
            "nodes": int(nodes),
            "rssi_a": int(rssi_a),
            "rssi_b": int(rssi_b)
        }
    return None

def main():
    print("Relay module handler starting...")

    while True:
        try:
            port = serial.Serial(MODULE_PORT, MODULE_BAUD, timeout=1)
            print(f"Connected to relay module on {MODULE_PORT}")
            break
        except serial.SerialException:
            print(f"Waiting for relay module on {MODULE_PORT}...")
            time.sleep(2)

    while True:
        try:
            if port.in_waiting > 0:
                line = port.readline().decode('utf-8', errors='ignore').strip()
                if line.startswith("RELAY_READY") or line.startswith("RELAY_STATUS"):
                    status = parse_relay_status(line)
                    if status:
                        print(f"Relay status: {status}")
                        with open(RELAY_STATUS_FILE, 'w') as f:
                            import json
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
