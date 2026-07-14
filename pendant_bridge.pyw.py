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

@sio.event
def connect():
    print(" [gSender]: Connected via Socket.IO WebSocket!")

@sio.event
def disconnect():
    print(" [gSender]: Disconnected from server.")

try:
    print(f"Connecting to gSender at {GSENDER_SERVER}...")
    sio.connect(GSENDER_SERVER)
except Exception as e:
    print(f"[ERROR] Could not connect to gSender WebSocket: {e}")
    exit()

print(f"Connecting to ESP32 on {ESP32_PORT}...")
try:
    ser = serial.Serial(ESP32_PORT, BAUD_RATE, timeout=0.01)
    print("\n--- PENDANT BRIDGE IS READY! ---")
    print("Tap for steps, hold for continuous jogging...\n")
except Exception as e:
    print(f"[ERROR] Could not open {ESP32_PORT}: {e}")
    sio.disconnect()
    exit()

while True:
    try:
        if ser.in_waiting > 0:
            cmd = ser.readline().decode('utf-8', errors='ignore').strip()
            
            if cmd and not cmd.startswith("E (") and not cmd.startswith("load:") and not cmd.startswith("entry"):
                
                # Preset mode notification + gSender UI sync
                if cmd.startswith("PRESET:"):
                    print(f"\n[MODE SWITCH]: {cmd}")
                    mode_name = cmd.split(" ")[1].lower()  # 'rapid', 'normal', 'precise'
                    
                    # Log comment to terminal console
                    sio.emit('command', (CNC_PORT, 'gcode', f"; {cmd}"))
                    
                    # Emit UI preset sync events
                    sio.emit('fe-pendant', {'action': 'set-jog-preset', 'preset': mode_name})
                    sio.emit('command', (CNC_PORT, 'jog-preset', mode_name))

                # Realtime Jog Cancel (0x85)
                elif cmd == "JOG_CANCEL":
                    print("[ACTION]: REALTIME JOG CANCEL (0x85)")
                    sio.emit('write', (CNC_PORT, '\x85'))
                
                # Soft Reset (0x18)
                elif cmd == "RESET":
                    print("[ACTION]: SOFT RESET (0x18)")
                    sio.emit('write', (CNC_PORT, '\x18'))
                
                # Standard G-code / Jog command
                else:
                    print(f"[PENDANT SENT]: {cmd}")
                    sio.emit('command', (CNC_PORT, 'gcode', cmd))

    except Exception as e:
        print(f"[LOOP ERROR]: {e}")
        
    time.sleep(0.005)