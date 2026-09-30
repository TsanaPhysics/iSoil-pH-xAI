#!/usr/bin/env python3
"""
Project: RBRU Digital Agriphysics & AI Soil pH Monitor
File: tools/serial_bridge.py
Description: USB-Serial to HTTP REST Bridge with Time Synchronization and Session Management
"""

import sys
import time
import glob
import re
import os
import json
import urllib.request
import urllib.parse
import serial

API_URL = "http://localhost/06_AI_Research/my_ph_wio/api/post_data.php"

def find_wio_port():
    ports = glob.glob('/dev/cu.usbmodem*')
    if ports:
        return ports[0]
    return None

def send_to_api(payload):
    try:
        data_bytes = json.dumps(payload).encode('utf-8')
        req = urllib.request.Request(
            API_URL, 
            data=data_bytes, 
            headers={'Content-Type': 'application/json'}
        )
        with urllib.request.urlopen(req, timeout=3) as resp:
            result = json.loads(resp.read().decode('utf-8'))
            return result.get('success', False)
    except Exception as e:
        print(f"[API ERROR] Failed to send: {e}")
        return False

def parse_line(line, active_session="EXP_001"):
    # ตัวอย่าง: Volt:1.6660 Temp:25.0 pH_Trad:7.12 pH_AI:6.91 Target:BUF 7.00 DateTime:2026-09-30 15:02:18 Session:EXP_001
    try:
        v_match = re.search(r'Volt:([0-9\.]+)', line)
        t_match = re.search(r'Temp:([0-9\.]+)', line)
        pt_match = re.search(r'pH_Trad:([0-9\.]+)', line)
        pa_match = re.search(r'pH_AI:([0-9\.]+)', line)
        buf_match = re.search(r'Target:([A-Za-z0-9_\. ]+)', line)
        sess_match = re.search(r'Session:([A-Za-z0-9_\-]+)', line)
        session_val = sess_match.group(1).strip() if sess_match else active_session

        mode_match = re.search(r'Mode:([^\t\r\n]+)', line)
        model_match = re.search(r'Model:([^\t\r\n]+)', line)
        mode_val = mode_match.group(1).strip() if mode_match else "FIELD_RUN"
        model_val = model_match.group(1).strip() if model_match else "ANN DURIAN"
        dt_match = re.search(r'DateTime:([0-9\-]+ [0-9:]+)', line)

        if v_match and pa_match:
            voltage = float(v_match.group(1))
            temp_c = float(t_match.group(1)) if t_match else 25.0
            ph_trad = float(pt_match.group(1)) if pt_match else float(pa_match.group(1))
            ph_ai = float(pa_match.group(1))
            target = buf_match.group(1).strip() if buf_match else "FIELD"
            datetime_val = dt_match.group(1).strip() if dt_match else time.strftime('%Y-%m-%d %H:%M:%S')

            return {
                "session_id": session_val,
                "model_name": model_val,
                "mode": mode_val,
                "voltage": voltage,
                "temp_c": temp_c,
                "ph_traditional": ph_trad,
                "ph_ai": ph_ai,
                "target_buffer": target,
                "datetime": datetime_val,
                "sample_note": f"MODE:{mode_val}|MODEL:{model_val}"
            }
    except Exception as e:
        pass
    return None

def main():
    print("[*] Starting RBRU Soil pH Serial Bridge with Time Sync & Session Manager...")
    
    cmd_file = os.path.join(os.path.dirname(__file__), '..', 'data', 'bridge_cmd.json')
    
    port = find_wio_port()
    if not port:
        print("[!] No Wio Terminal found on /dev/cu.usbmodem*. Retrying every 3s...")
        while not port:
            time.sleep(3)
            port = find_wio_port()
    
    print(f"[*] Found Wio Terminal at {port}. Opening serial connection @ 115200 bps...")
    
    active_session = "EXP_001"
    
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
                    # ตรวจสอบว่ามีคำสั่งจาก Web Dashboard หรือไม่
                    if os.path.exists(cmd_file):
                        try:
                            with open(cmd_file, 'r', encoding='utf-8') as f:
                                cmd_info = json.load(f)
                            os.remove(cmd_file)
                            if cmd_info.get('command') == 'START_NEW':
                                new_sess = cmd_info.get('session_id', '')
                                active_session = new_sess
                                ser.write(f"START_NEW:{new_sess}\n".encode('utf-8'))
                                print(f"\n[BRIDGE CMD] Dispatched START_NEW:{new_sess} to Wio Terminal\n")
                        except Exception as ce:
                            print(f"[BRIDGE CMD ERROR] {ce}")

                    # ซิงค์เวลาซ้ำทุก 60 วินาที เพื่อป้องกันเวลานาฬิกาคลาดเคลื่อน
                    if time.time() - last_sync_time >= 60.0:
                        last_sync_time = time.time()
                        now_str = time.strftime('%Y-%m-%d %H:%M:%S')
                        ser.write(f"TIME:{now_str}\n".encode('utf-8'))

                    raw_line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if raw_line:
                        if raw_line.startswith("NEW_SESSION:"):
                            active_session = raw_line.split(":", 1)[1].strip()
                            print(f"\n[SESSION NOTIFICATION] Wio Terminal started: {active_session}\n")
                            continue

                        data = parse_line(raw_line, active_session)
                        if data:
                            now = time.time()
                            # ส่งเข้าฐานข้อมูลทุกๆ 1.5 วินาที
                            if now - last_post_time >= 1.5:
                                last_post_time = now
                                success = send_to_api(data)
                                status_tag = "✓ SAVED" if success else "✗ FAIL"
                                print(f"[{data['datetime']}] {status_tag} | [{data['session_id']}] [{data['mode']}] [{data['model_name']}] pH_AI: {data['ph_ai']:.2f} | Volt: {data['voltage']:.3f}V | Temp: {data['temp_c']}C")
        except (serial.SerialException, OSError) as e:
            print(f"[!] Serial disconnected: {e}. Reconnecting in 3s...")
            time.sleep(3)
            port = find_wio_port()
        except KeyboardInterrupt:
            print("\n[*] Stopping Serial Bridge.")
            sys.exit(0)

if __name__ == "__main__":
    main()
