from pymavlink import mavutil
import time
import json
import threading

# Shared state dictionary — all other scripts read from this
state = {
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
    }
}

# Pixhawk UART connection
PIXHAWK_PORT = '/dev/ttyAMA0'
PIXHAWK_BAUD = 57600

connection = None

def connect_pixhawk():
    global connection
    while True:
        try:
            print(f"Connecting to Pixhawk on {PIXHAWK_PORT}...")
            connection = mavutil.mavlink_connection(
                PIXHAWK_PORT,
                baud=PIXHAWK_BAUD
            )
            print("Waiting for heartbeat...")
            connection.wait_heartbeat()
            print("Pixhawk connected!")
            state["drone"]["connected"] = True
            break
        except Exception as e:
            print(f"Connection failed: {e}")
            time.sleep(3)

def read_telemetry():
    global connection
    while True:
        try:
            msg = connection.recv_match(
                type=[
                    'HEARTBEAT',
                    'GLOBAL_POSITION_INT',
                    'BATTERY_STATUS',
                    'ATTITUDE',
                    'SYS_STATUS'
                ],
                blocking=True,
                timeout=2.0
            )

            if msg is None:
                state["drone"]["connected"] = False
                print("Pixhawk timeout — reconnecting...")
                connect_pixhawk()
                continue

            state["drone"]["connected"] = True
            msg_type = msg.get_type()

            if msg_type == 'HEARTBEAT':
                state["drone"]["armed"] = bool(
                    msg.base_mode & 
                    mavutil.mavlink.MAV_MODE_FLAG_SAFETY_ARMED
                )
                state["drone"]["mode"] = mavutil.mode_string_v10(msg)

            elif msg_type == 'GLOBAL_POSITION_INT':
                state["drone"]["lat"] = msg.lat / 1e7
                state["drone"]["lon"] = msg.lon / 1e7
                state["drone"]["alt"] = msg.relative_alt / 1000.0

            elif msg_type == 'BATTERY_STATUS':
                if msg.battery_remaining >= 0:
                    state["drone"]["battery"] = msg.battery_remaining

            elif msg_type == 'ATTITUDE':
                import math
                state["drone"]["roll"] = round(
                    math.degrees(msg.roll), 1)
                state["drone"]["pitch"] = round(
                    math.degrees(msg.pitch), 1)
                state["drone"]["yaw"] = round(
                    math.degrees(msg.yaw), 1)

        except Exception as e:
            print(f"Telemetry error: {e}")
            time.sleep(1)

def get_state():
    return state

def main():
    print("MAVLink bridge starting...")
    connect_pixhawk()
    read_telemetry()

if __name__ == "__main__":
    main()
