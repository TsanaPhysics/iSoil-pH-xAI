#!/usr/bin/env python3
"""
Project: RBRU Digital Agriphysics & AI Soil pH Monitor
File: tools/serial_bridge.py
Description: Background bridge connecting Seeed Studio Wio Terminal USB Serial
             to the SQLite3 Database & Web Dashboard via REST API with Real-time Clock Sync.
"""

import sys
import time
import glob
import re
import urllib.request
import json
import serial

API_ENDPOINT = "http://localhost/06_AI_Research/my_ph_wio/api/post_data.php"

def find_wio_port():
    ports = glob.glob('/dev/cu.usbmodem*')
    if ports:
        return ports[0]
    return None

def send_to_api(payload):
    try:
        req = urllib.request.Request(
            API_ENDPOINT,
            data=json.dumps(payload).encode('utf-8'),
            headers={'Content-Type': 'application/json'}
        )
        with urllib.request.urlopen(req, timeout=3) as resp:
            result = json.loads(resp.read().decode('utf-8'))
            return result.get('success', False)
    except Exception as e:
        print(f"[API ERROR] Failed to send: {e}")
        return False

def parse_line(line):
    # ตัวอย่าง: Volt:1.6660 Temp:25.0 pH_Trad:7.12 pH_AI:6.91 Target:BUF 7.00 DateTime:2026-09-30 15:02:18
    try:
        v_match = re.search(r'Volt:([0-9\.]+)', line)
        t_match = re.search(r'Temp:([0-9\.]+)', line)
        pt_match = re.search(r'pH_Trad:([0-9\.]+)', line)
        pa_match = re.search(r'pH_AI:([0-9\.]+)', line)
        buf_match = re.search(r'Target:([A-Za-z0-9_\. ]+)', line)
        dt_match = re.search(r'DateTime:([0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2})', line)

        if v_match and pa_match:
            voltage = float(v_match.group(1))
            temp_c = float(t_match.group(1)) if t_match else 25.0
            ph_trad = float(pt_match.group(1)) if pt_match else float(pa_match.group(1))
            ph_ai = float(pa_match.group(1))
            target = buf_match.group(1).strip() if buf_match else "FIELD"
            datetime_val = dt_match.group(1).strip() if dt_match else time.strftime('%Y-%m-%d %H:%M:%S')

            return {
                "voltage": voltage,
                "temp_c": temp_c,
                "ph_traditional": ph_trad,
                "ph_ai": ph_ai,
                "target_buffer": target,
                "datetime": datetime_val,
                "sample_note": "LIVE_WIO_STREAM"
            }
    except Exception as e:
        pass
    return None

def main():
    print("[*] Starting RBRU Soil pH Serial Bridge with Time Sync...")
    
    port = find_wio_port()
    if not port:
        print("[!] No Wio Terminal found on /dev/cu.usbmodem*. Retrying every 3s...")
        while not port:
            time.sleep(3)
            port = find_wio_port()
    
    print(f"[*] Found Wio Terminal at {port}. Opening serial connection @ 115200 bps...")
    
    while True:
        try:
            with serial.Serial(port, 115200, timeout=2) as ser:
                print(f"[+] Connected to {port}! Syncing date & time and streaming data...")
                
                # ส่งคำสั่งซิงค์วันและเวลากับเครื่องคอมพิวเตอร์ทันทีที่เชื่อมต่อ
                now_str = time.strftime('%Y-%m-%d %H:%M:%S')
                ser.write(f"TIME:{now_str}\n".encode('utf-8'))
                
                last_post_time = 0
                last_sync_time = time.time()
                
                while True:
                    # ซิงค์เวลาซ้ำทุก 60 วินาที เพื่อป้องกันเวลานาฬิกาคลาดเคลื่อน
                    if time.time() - last_sync_time >= 60.0:
                        last_sync_time = time.time()
                        now_str = time.strftime('%Y-%m-%d %H:%M:%S')
                        ser.write(f"TIME:{now_str}\n".encode('utf-8'))

                    raw_line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if raw_line:
                        data = parse_line(raw_line)
                        if data:
                            now = time.time()
                            # ส่งเข้าฐานข้อมูลทุกๆ 1.5 วินาที
                            if now - last_post_time >= 1.5:
                                last_post_time = now
                                success = send_to_api(data)
                                status_tag = "✓ SAVED" if success else "✗ FAIL"
                                print(f"[{data['datetime']}] {status_tag} | pH_AI: {data['ph_ai']:.2f} | Volt: {data['voltage']:.3f}V | Temp: {data['temp_c']}C | Target: {data['target_buffer']}")
        except (serial.SerialException, OSError) as e:
            print(f"[!] Serial disconnected: {e}. Reconnecting in 3s...")
            time.sleep(3)
            port = find_wio_port()
        except KeyboardInterrupt:
            print("\n[*] Stopping Serial Bridge.")
            sys.exit(0)

if __name__ == "__main__":
    main()
