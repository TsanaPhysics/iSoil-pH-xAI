#!/usr/bin/env python3
"""
Project: Wio Terminal Soil pH Research - Serial Telemetry Logger
Description: Reads real-time AI and Traditional pH data from Wio Terminal USB Serial
             and logs to a local CSV file for research analysis.
Author: RBRU Digital Agriphysics & AI Research Team
"""

import sys
import time
import glob
import csv
import serial

def find_wio_port():
    ports = glob.glob('/dev/cu.usbmodem*')
    if ports:
        return ports[0]
    return None

def main():
    port = find_wio_port()
    if not port:
        print("[ERROR] No Wio Terminal detected on /dev/cu.usbmodem*")
        sys.exit(1)

    baud = 115200
    print(f"[*] Connecting to Wio Terminal on {port} @ {baud} bps...")
    
    try:
        ser = serial.Serial(port, baud, timeout=2)
        time.sleep(1.5)
        print("[*] Connected! Streaming live telemetry (Press Ctrl+C to stop)...")
        print("-" * 75)
        
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"[{time.strftime('%H:%M:%S')}] {line}")
    except KeyboardInterrupt:
        print("\n[*] Logging stopped by user.")
    except Exception as e:
        print(f"[!] Error: {e}")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == '__main__':
    main()
