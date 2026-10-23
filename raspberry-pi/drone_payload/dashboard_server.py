import asyncio
import websockets
import json
import time
import threading
import os
import re
from flask import Flask, send_file, request, jsonify
RELAY_STATUS_FILE = '/tmp/relay_status.txt'
TETHER_STATUS_FILE = '/tmp/tether_status.txt'
TETHER_COMMAND_FILE = '/tmp/tether_command.txt'
# Shared state — updated by mavlink_bridge and payload_manager
dashboard_state = {
    "drone": {
        "armed": False,
        "mode": "UNKNOWN",
        "battery": 0,
        "lat": 0.0,
        "lon": 0.0,
        "alt": 0.0,
        "roll": 0.0,
        "pitch": 0.0,
        "yaw": 0.0,
        "connected": False
    },
    "module": {
        "connected": False,
        "type": "none",
        "name": "No module",
        "current_ma": 0,
        "status": "idle",
        "data": {},
        "stream_url": None,
	"relay_data": None,
	"tether_data": None
    },
    "faults": []
}

connected_clients = set()

CAMERA_STREAM_FILE = '/tmp/camera_stream.txt'

def parse_camera_ready(line):
    # CAMERA_READY:ip=192.168.72.45:port=80:path=/stream
    match = re.match(r'CAMERA_READY:ip=([\d.]+):port=(\d+):path=(\S+)', line)
    if match:
        ip, port, path = match.groups()
        return f"http://{ip}:{port}{path}"
    return None

def check_camera_stream():
    if os.path.exists(CAMERA_STREAM_FILE):
        try:
            with open(CAMERA_STREAM_FILE, 'r') as f:
                line = f.read().strip()
            stream_url = parse_camera_ready(line)
            if stream_url:
                dashboard_state["module"]["stream_url"] = stream_url
        except Exception as e:
            print(f"Error reading camera stream file: {e}")

def check_relay_status():
    if os.path.exists(RELAY_STATUS_FILE):
        try:
            with open(RELAY_STATUS_FILE, 'r') as f:
                data = json.load(f)
            dashboard_state["module"]["relay_data"] = data
        except Exception as e:
            print(f"Error reading relay status: {e}")

def check_tether_status():
    if os.path.exists(TETHER_STATUS_FILE):
        try:
            with open(TETHER_STATUS_FILE, 'r') as f:
                data = json.load(f)
            dashboard_state["module"]["tether_data"] = data
        except Exception as e:
            print(f"Error reading tether status: {e}")

async def handler(websocket):
    connected_clients.add(websocket)
    print(f"Dashboard client connected")
    try:
        while True:
            check_camera_stream()
	    check_relay_status()
	    check_tether_status()
            await websocket.send(json.dumps(dashboard_state))
            await asyncio.sleep(1)
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        connected_clients.discard(websocket)
        print("Dashboard client disconnected")

async def start_websocket():
    print("WebSocket server starting on port 8765...")
    async with websockets.serve(handler, "0.0.0.0", 8765):
        await asyncio.Future()

# Flask server for the HTML dashboard
app = Flask(__name__)

@app.route('/')
def index():
    return send_file('dashboard.html')
@app.route('/tether_command', methods=['POST'])

def tether_command():
    command = request.json.get('command', '')
    if command:
        with open(TETHER_COMMAND_FILE, 'w') as f:
            f.write(command)
        return jsonify({"status": "ok"})
    return jsonify({"status": "error"}), 400

def start_flask():
    app.run(host='0.0.0.0', port=8080, debug=False)

def update_drone_state(drone_data):
    dashboard_state["drone"].update(drone_data)

def update_module_state(module_data):
    dashboard_state["module"].update(module_data)

def add_fault(fault_message):
    dashboard_state["faults"].append({
        "time": time.strftime("%H:%M:%S"),
        "message": fault_message
    })
    if len(dashboard_state["faults"]) > 10:
        dashboard_state["faults"].pop(0)

def clear_faults():
    dashboard_state["faults"] = []

def main():
    print("Dashboard server starting...")

    flask_thread = threading.Thread(target=start_flask, daemon=True)
    flask_thread.start()

    asyncio.run(start_websocket())

if __name__ == "__main__":
    main()
