import serial
import serial.tools.list_ports
import os
import sys
import time  # Added for stream capture timing

BAUDRATE = 115200
REQUEST_COMMAND = b"GET_FILE\n"
REQUEST_LINKED_LIST = b"GET_LINKED_LIST\n"
SAVE_DIR = "DataFolder"
SAVE_FILE = "TestFileDownload.txt"
LINKED_LIST_FILE = "health_data_linked_list.txt"

# STM32 VID/PID - Basic example values
STM32_VID = 0x0483  # STMicroelectronics VID
STM32_PID = 0x374E  # STM32 Virtual COM Port PID

def find_uart_device(vid, pid):
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if vid and pid:
            if port.vid == vid and port.pid == pid:
                return port.device
        else:
            return port.device
    return None

def test_device_connection():
    """Test connection to the device and verify it's responding"""
    print("=== Device Connection Test ===")
    
    # First, list all available devices
    list_all_serial_devices()
    
    # Try to find the STM32 device
    port = find_uart_device(STM32_VID, STM32_PID)
    
    if not port:
        print("No STM32 device found with specified VID/PID. Trying first available port...")
        # If no specific device found, try the first available port
        port = find_uart_device(None, None)
    
    if not port:
        print("No serial devices found at all!")
        return False
    
    print(f"Testing connection on port: {port}")
    
    try:
        with serial.Serial(port, BAUDRATE, timeout=2) as ser:
            print("Serial connection established successfully!")
            
            # Clear buffer
            ser.reset_input_buffer()
            
            # Test basic communication
            print("Testing basic communication...")
            ser.write(b"HELLO\n")
            
            # Try to read response
            response = ser.readline()
            if response:
                print(f"Device responded: {response.decode('utf-8', errors='ignore').strip()}")
            else:
                print("No response from device (this might be normal if device doesn't echo)")
            
            print("Connection test completed successfully!")
            return True
            
    except Exception as e:
        print(f"Connection test failed: {e}")
        return False

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
    """Capture streaming data from device and save to file"""
    os.makedirs(SAVE_DIR, exist_ok=True)
    save_path = os.path.join(SAVE_DIR, SAVE_FILE)
    try:
        with serial.Serial(port, BAUDRATE, timeout=2) as ser:  # Reduced timeout for stream capture
            # Clear any existing data in buffer first
            ser.reset_input_buffer()
            
            # Send the GET_FILE command first
            ser.write(REQUEST_COMMAND)
            print("GET_FILE command sent. Listening for response...")
            
            stream_data = []
            start_time = time.time()
            capture_duration = 10  # Capture for 10 seconds
            
            while time.time() - start_time < capture_duration:
                data = ser.read(1024)
                if data:
                    stream_data.append(data)
                    print(f"Received {len(data)} bytes...")
                else:
                    print("No data received in timeout period")
            
            # Save all captured stream data
            with open(save_path, "wb") as f:
                for chunk in stream_data:
                    f.write(chunk)
        
        print(f"Stream data captured successfully to {save_path}")
        print(f"Captured {len(stream_data)} data chunks over {capture_duration} seconds")
        return save_path
    except Exception as e:
        print(f"Error: {e}")
        return None

def download_linked_list_from_device(port):
    """Read linked list data from device and save to health_data format"""
    os.makedirs(SAVE_DIR, exist_ok=True)
    save_path = os.path.join(SAVE_DIR, LINKED_LIST_FILE)
    
    try:
        with serial.Serial(port, BAUDRATE, timeout=5) as ser:
            # Clear any existing data in buffer first
            ser.reset_input_buffer()
            
            # Send the GET_LINKED_LIST command
            ser.write(REQUEST_LINKED_LIST)
            print("GET_LINKED_LIST command sent. Waiting for response...")
            
            # Wait for acknowledgment or start of data
            ack_data = ser.readline()
            print(f"Device response: {ack_data.decode('utf-8', errors='ignore').strip()}")
            
            linked_list_data = []
            print("Reading linked list data from device...")
            
            # Read linked list entries until we get an end marker or timeout
            while True:
                try:
                    # Read a line of data from the device
                    raw_line = ser.readline()
                    if not raw_line:
                        print("No more data received - ending read")
                        break
                    
                    line = raw_line.decode('utf-8', errors='ignore').strip()
                    
                    # Check for end marker
                    if line == "END_OF_LIST" or line == "":
                        print("End of linked list detected")
                        break
                    
                    # Parse linked list entry and convert to health data format
                    health_entry = parse_linked_list_entry(line)
                    if health_entry:
                        linked_list_data.append(health_entry)
                        print(f"Parsed entry: {health_entry}")
                    
                except UnicodeDecodeError:
                    print("Unicode decode error - skipping line")
                    continue
                except Exception as e:
                    print(f"Error reading line: {e}")
                    continue
            
            # Save the linked list data in health_data format
            with open(save_path, "w") as f:
                for entry in linked_list_data:
                    f.write(entry + "\n")
            
            print(f"Linked list data saved successfully to {save_path}")
            print(f"Total entries processed: {len(linked_list_data)}")
            return save_path
            
    except Exception as e:
        print(f"Error reading linked list from device: {e}")
        return None

def parse_linked_list_entry(raw_entry):
    """
    Parse a linked list entry from the device and convert to health data format.
    Expected input format from device might be something like:
    "timestamp:1234567890,hr:75,sc:1100.5,status:Normal,next:0x12345678"
    
    Output format should be: "HH:MM:SS Heart_Rate Skin_Conductivity Episode"
    """
    try:
        # This is a placeholder parser - you'll need to adjust based on your actual device format
        # For now, let's assume the device sends comma-separated key:value pairs
        
        entry_data = {}
        if ',' in raw_entry:
            # Parse comma-separated key:value pairs
            pairs = raw_entry.split(',')
            for pair in pairs:
                if ':' in pair:
                    key, value = pair.split(':', 1)
                    entry_data[key.strip()] = value.strip()
        else:
            # If it's already in the correct format, return as-is
            return raw_entry
        
        # Extract values (adjust keys based on your device's actual format)
        timestamp = entry_data.get('timestamp', entry_data.get('time', '00:00:00'))
        # Removed temperature extraction
        heart_rate = entry_data.get('hr', entry_data.get('heart_rate', '0.0'))
        skin_conductivity = entry_data.get('sc', entry_data.get('skin_conductivity', '0.0'))
        episode = entry_data.get('status', entry_data.get('episode', 'Normal'))
        
        # Convert timestamp if it's a unix timestamp
        if timestamp.isdigit() and len(timestamp) > 8:
            # Convert unix timestamp to HH:MM:SS format
            import datetime
            dt = datetime.datetime.fromtimestamp(int(timestamp))
            timestamp = dt.strftime("%H:%M:%S")
        elif ':' not in timestamp:
            # If it's seconds since start, convert to HH:MM:SS
            try:
                seconds = int(float(timestamp))
                hours = seconds // 3600
                minutes = (seconds % 3600) // 60
                secs = seconds % 60
                timestamp = f"{hours:02d}:{minutes:02d}:{secs:02d}"
            except:
                timestamp = "00:00:00"
        
        # Format as health data entry (removed temperature)
        health_entry = f"{timestamp} {heart_rate} {skin_conductivity} {episode}"
        return health_entry
        
    except Exception as e:
        print(f"Error parsing linked list entry '{raw_entry}': {e}")
        return None

# Original file-based download function (commented out for later use)
"""
def download_file_from_device_original(port):
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
"""