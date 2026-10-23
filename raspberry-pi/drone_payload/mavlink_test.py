from pymavlink import mavutil
import time

print("Connecting to Pixhawk...")

connection = mavutil.mavlink_connection(
    '/dev/ttyAMA0',
    baud=57600
)

print("Waiting for heartbeat...")
connection.wait_heartbeat()
print("Heartbeat received!")
print(f"System: {connection.target_system}")
print(f"Component: {connection.target_component}")

while True:
    msg = connection.recv_match(
        type=['GLOBAL_POSITION_INT', 'BATTERY_STATUS', 'ATTITUDE'],
        blocking=True,
        timeout=2.0
    )
    if msg:
        print(msg)
    time.sleep(0.5)
