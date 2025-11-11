#!/usr/bin/env python3
"""
Test script for linked list data collection from device.
This script tests the connection and linked list reading functionality.
"""

from SerialConnection import (
    test_device_connection, 
    download_linked_list_from_device, 
    find_uart_device,
    list_all_serial_devices,
    parse_linked_list_entry
)
import os

def test_parser():
    """Test the linked list entry parser with sample data"""
    print("\n=== Testing Linked List Parser ===")
    
    # Test various input formats
    test_entries = [
        "timestamp:1699276800,temp:28.5,hr:75,sc:1100.5,status:Normal,next:0x12345678",
        "time:10:30:15,temperature:29.1,heart_rate:78,skin_conductivity:1095.2,episode:Normal",
        "1234567890,28.7,76,1102.3,Normal",  # comma-separated without keys
        "10:30:15 28.91 76.0 1103.56 Normal",  # already in correct format
    ]
    
    for i, entry in enumerate(test_entries):
        print(f"\nTest {i+1}: {entry}")
        parsed = parse_linked_list_entry(entry)
        print(f"Parsed: {parsed}")

def main():
    print("=== CoolWrist Linked List Connection Test ===")
    print("This script will test the connection to your device and")
    print("verify that linked list reading functionality works.\n")
    
    # Test the parser first
    test_parser()
    
    # Test device connection
    print("\n=== Testing Device Connection ===")
    connection_ok = test_device_connection()
    
    if not connection_ok:
        print("\nConnection test failed. Please:")
        print("1. Make sure your device is connected")
        print("2. Check if the device drivers are installed")
        print("3. Verify the VID/PID values are correct for your device")
        return
    
    # Ask user if they want to test linked list download
    print("\n=== Ready for Linked List Test ===")
    user_input = input("Do you want to test linked list download from the device? (y/n): ")
    
    if user_input.lower() in ['y', 'yes']:
        print("Testing linked list download...")
        
        # Find the device port
        port = find_uart_device(vid=0x0483, pid=0x374E)
        if not port:
            port = find_uart_device(None, None)  # Try first available
        
        if port:
            print(f"Using port: {port}")
            result = download_linked_list_from_device(port)
            
            if result:
                print(f"\nLinked list download test completed!")
                print(f"Data saved to: {result}")
                
                # Check if file was created and show first few lines
                if os.path.exists(result):
                    with open(result, 'r') as f:
                        lines = f.readlines()[:5]  # First 5 lines
                    print(f"\nFirst few lines of downloaded data:")
                    for line in lines:
                        print(f"  {line.strip()}")
            else:
                print("Linked list download test failed!")
        else:
            print("No device port found!")
    else:
        print("Skipping linked list download test.")
    
    print("\n=== Test Complete ===")
    print("If everything worked correctly, you can now use the main app")
    print("to download linked list data from your device!")

if __name__ == "__main__":
    main()