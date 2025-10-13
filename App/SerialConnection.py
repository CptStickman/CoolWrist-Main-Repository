import serial
import serial.tools.list_ports
import os

BAUDRATE = 115200
REQUEST_COMMAND = b"GET_FILE\n"
SAVE_DIR = "DataFolder"
SAVE_FILE = "TestFileDownload.txt"

# STM32 VID/PID - Basic example values
STM32_VID = 0x0483  # STMicroelectronics VID
STM32_PID = 0x5740  # STM32 Virtual COM Port PID

def find_uart_device(vid = None, pid = None):
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if vid and pid:
            if port.vid == vid and port.pid == pid:
                return port.device
        else:
            return port.device
    return None

def list_all_serial_devices():
    """List all serial devices with their VID/PID for debugging"""
    ports = serial.tools.list_ports.comports()
    print("Available serial devices:")
    for port in ports:
        print(f"Port: {port.device}")
        print(f"  Description: {port.description}")
        print(f"  VID: 0x{port.vid:04X} ({port.vid})" if port.vid else "  VID: None")
        print(f"  PID: 0x{port.pid:04X} ({port.pid})" if port.pid else "  PID: None")
        print(f"  Serial Number: {port.serial_number}")
        print(f"  Manufacturer: {port.manufacturer}")
        print("---")

def download_file_from_device(port):
    os.makedirs(SAVE_DIR, exist_ok=True)
    save_path = os.path.join(SAVE_DIR, SAVE_FILE)
    try:
        with serial.Serial(port, BAUDRATE, timeout=5) as ser:
            ser.write(REQUEST_COMMAND)
            print("Request Sent. Receiving data...")
            
            with open(save_path, "wb") as f:
                while True:
                    data = ser.read(1024)
                    if not data:
                        break
                    f.write(data)
        print(f"File downloaded successfully to {save_path}")
    except Exception as e:
        print(f"Error: {e}")
        return None