#!/usr/bin/env python3
"""Receive the robot telemetry (UDP) on the laptop.

Prints every message and logs SCAN lines to a CSV file for later plotting.

    python tools/udp_listener.py [--port 4210] [--csv scan_log.csv]

Message formats sent by the robot:
    SCAN,<robot_ms>,<servo_angle_deg>,<distance_cm>,<signal_strength>
    ACTION,<EVENT_NAME>

On Windows, allow Python through the firewall for UDP on this port (private network).
"""
import argparse
import csv
import socket
import time


def main() -> None:
    parser = argparse.ArgumentParser(description="Robot UDP telemetry listener")
    parser.add_argument("--port", type=int, default=4210, help="UDP port (default 4210)")
    parser.add_argument("--csv", default="scan_log.csv", help="CSV file for SCAN readings")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", args.port))
    print(f"Listening for robot telemetry on UDP port {args.port} (Ctrl+C to stop)")

    with open(args.csv, "a", newline="") as f:
        writer = csv.writer(f)
        if f.tell() == 0:
            writer.writerow(["pc_time", "robot_ms", "angle_deg", "distance_cm", "strength"])
        try:
            while True:
                data, addr = sock.recvfrom(1024)
                line = data.decode(errors="replace").strip()
                print(f"[{addr[0]}] {line}")
                parts = line.split(",")
                if parts[0] == "SCAN" and len(parts) == 5:
                    writer.writerow([time.strftime("%H:%M:%S")] + parts[1:])
                    f.flush()
        except KeyboardInterrupt:
            print("\nStopped.")


if __name__ == "__main__":
    main()
