import serial
import subprocess
import os
import signal
import time
import json

# UART connection to ESP32
ESP32_PORT = '/dev/ttyUSB0'
ESP32_BAUD = 115200

# Module config directory
CONFIG_DIR = '/home/chinmay/drone_payload/modules'

# Currently running module process
current_process = None
current_module_type = None
max_current_ma = 1000

def load_module_config(type_id):
    config_path = f"{CONFIG_DIR}/module_type_{type_id}.json"
    if os.path.exists(config_path):
        with open(config_path, 'r') as f:
            return json.load(f)
    return None

def start_module(config):
    global current_process
    script = f"/home/chinmay/drone_payload/{config['script']}"
    if os.path.exists(script):
        current_process = subprocess.Popen(
            ['python3', script],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        print(f"Started module: {config['name']}")
    else:
        print(f"Module script not found: {script}")

def stop_module():
    global current_process, current_module_type
    if current_process:
        current_process.send_signal(signal.SIGTERM)
        try:
            current_process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            current_process.kill()
        current_process = None
        current_module_type = None
        print("Module stopped")

def handle_message(message):
    global current_module_type, max_current_ma
    message = message.strip()
    print(f"ESP32: {message}")

    if message.startswith("MODULE_CONNECTED"):
        parts = message.split(':')
        data = {}
        for part in parts[1:]:
            if '=' in part:
                key, value = part.split('=', 1)
                data[key] = value
        type_id = int(data.get('type', 0))
        max_current_ma = int(data.get('current_limit', 1000))
        current_module_type = type_id
        config = load_module_config(type_id)
        if config:
            start_module(config)
        else:
            print(f"No config for module type {type_id}")

    elif message.startswith("CAMERA_READY"):
        with open('/tmp/camera_stream.txt', 'w') as f:
            f.write(message)

    elif message == "MODULE_DISCONNECTED":
        stop_module()
        print("Module disconnected")

    elif message.startswith("FAULT"):
        stop_module()
        print(f"Fault detected: {message}")

def main():
    print("Payload manager starting...")

    # Create config directory
    os.makedirs(CONFIG_DIR, exist_ok=True)

    # Connect to ESP32
    while True:
        try:
            esp32 = serial.Serial(ESP32_PORT, ESP32_BAUD, timeout=1)
            print(f"Connected to ESP32 on {ESP32_PORT}")
            break
        except serial.SerialException:
            print(f"Waiting for ESP32 on {ESP32_PORT}...")
            time.sleep(2)

    # Main loop
    while True:
        try:
            if esp32.in_waiting > 0:
                line = esp32.readline().decode('utf-8', errors='ignore')
                if line.strip():
                    handle_message(line)

            # Check if module process crashed
            if current_process and current_process.poll() is not None:
                print("Module process crashed, restarting...")
                config = load_module_config(current_module_type)
                if config:
                    start_module(config)

            time.sleep(0.1)

        except KeyboardInterrupt:
            print("Shutting down...")
            stop_module()
            esp32.close()
            break
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(1)

if __name__ == "__main__":
    main()
