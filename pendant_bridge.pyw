import serial
import socketio
import time

# ============================================================================
# CONFIGURATION
# ============================================================================
ESP32_PORT     = 'COM8'                      # ESP32 Pendant port
CNC_PORT       = 'COM10'                     # CNC Machine port in gSender
BAUD_RATE      = 115200
GSENDER_SERVER = "http://192.168.178.32:8000" # gSender Server address

sio = socketio.Client()

# Connect to gSender WebSocket with automatic reconnection enabled
def connect_gsender():
    while not sio.connected:
        try:
            sio.connect(GSENDER_SERVER)
            print(" [gSender]: Connected via Socket.IO WebSocket!")
        except Exception:
            time.sleep(2)

connect_gsender()

# ============================================================================
# MAIN RECONNECT LOOP
# ============================================================================
while True:
    ser = None
    
    # 1. Attempt to connect to ESP32
    try:
        ser = serial.Serial(ESP32_PORT, BAUD_RATE, timeout=0.01)
        print(f"\n[CONNECTED]: ESP32 on {ESP32_PORT} is online!")
    except Exception:
        # ESP32 is unplugged/disconnected, wait 2 seconds and try again
        time.sleep(2)
        continue

    # 2. Main reading loop while ESP32 is plugged in
    while True:
        try:
            # Reconnect to gSender if server was restarted
            if not sio.connected:
                connect_gsender()

            if ser.in_waiting > 0:
                cmd = ser.readline().decode('utf-8', errors='ignore').strip()
                
                if cmd and not cmd.startswith("E (") and not cmd.startswith("load:") and not cmd.startswith("entry"):
                    
                    # Preset mode notification + gSender UI sync
                    if cmd.startswith("PRESET:"):
                        mode_name = cmd.split(" ")[1].lower()
                        sio.emit('command', (CNC_PORT, 'gcode', f"; {cmd}"))
                        sio.emit('fe-pendant', {'action': 'set-jog-preset', 'preset': mode_name})
                        sio.emit('command', (CNC_PORT, 'jog-preset', mode_name))

                    # Realtime Jog Cancel (0x85)
                    elif cmd == "JOG_CANCEL":
                        sio.emit('write', (CNC_PORT, '\x85'))
                    
                    # Soft Reset (0x18)
                    elif cmd == "RESET":
                        sio.emit('write', (CNC_PORT, '\x18'))
                    
                    # Standard G-code / Jog command
                    else:
                        sio.emit('command', (CNC_PORT, 'gcode', cmd))

            time.sleep(0.005)

        except (serial.SerialException, OSError):
            # ESP32 was disconnected mid-operation
            print("[DISCONNECTED]: ESP32 unplugged. Waiting for reconnection...")
            if ser and ser.is_open:
                ser.close()
            break # Break inner loop to go back to auto-reconnect outer loop
            
        except Exception as e:
            time.sleep(0.01)
