import serial
import time

port = 'COM4'
baudrate = 115200
timeout = 8

try:
    ser = serial.Serial(port, baudrate, timeout=1)
    
    # Reset ESP32-S3
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.dtr = True
    ser.rts = False
    
    print(f"--- Resetting and listening on {port} at {baudrate} baud for {timeout} seconds ---")
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(line)
        time.sleep(0.01)
        
    ser.close()
    print("--- Finished ---")
except Exception as e:
    print(f"Error opening serial port: {e}")
