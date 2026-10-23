import subprocess
import sys
import os
import time
import signal

# Paths to all scripts
BASE_DIR = '/home/chinmay/drone_payload'
SCRIPTS = [
    'mavlink_bridge.py',
    'payload_manager.py',
    'dashboard_server.py'
]

processes = []

def start_all():
    for script in SCRIPTS:
        path = os.path.join(BASE_DIR, script)
        print(f"Starting {script}...")
        p = subprocess.Popen(
            ['python3', path],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT
        )
        processes.append((script, p))
        time.sleep(1)
    print("All processes started")

def monitor():
    while True:
        for i, (name, p) in enumerate(processes):
            if p.poll() is not None:
                print(f"{name} crashed — restarting...")
                path = os.path.join(BASE_DIR, name)
                new_p = subprocess.Popen(
                    ['python3', path],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT
                )
                processes[i] = (name, new_p)
        time.sleep(5)

def shutdown(sig, frame):
    print("Shutting down...")
    for name, p in processes:
        print(f"Stopping {name}...")
        p.terminate()
    sys.exit(0)

signal.signal(signal.SIGINT, shutdown)
signal.signal(signal.SIGTERM, shutdown)

if __name__ == "__main__":
    print("Drone payload system starting...")
    start_all()
    monitor()
