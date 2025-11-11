# Linked List Data Collection Setup

## Overview
This setup allows the CoolWrist app to read health data stored as a linked list on the connected device and save it in the same format that works with the existing PlotData functions.

## What We've Added

### 1. SerialConnection.py Enhancements
- **New constant**: `REQUEST_LINKED_LIST = b"GET_LINKED_LIST\n"` - Command to request linked list data
- **New constant**: `LINKED_LIST_FILE = "health_data_linked_list.txt"` - Output filename
- **New function**: `download_linked_list_from_device(port)` - Downloads and converts linked list data
- **New function**: `parse_linked_list_entry(raw_entry)` - Converts device format to health data format
- **New function**: `test_device_connection()` - Tests device connectivity

### 2. AppCode.py Enhancements
- **New function**: `handle_linked_list_download()` - GUI handler for linked list download
- **New function**: `handle_test_connection()` - GUI handler for connection testing
- **New button**: "Download Health Data" - Downloads linked list data from device
- **New button**: "Test Connection" - Tests device connection

### 3. Test Script
- **New file**: `test_linked_list_connection.py` - Standalone test script

## How It Works

### Data Flow
1. Device stores health data as a linked list
2. App sends `GET_LINKED_LIST` command to device
3. Device responds with linked list entries
4. App parses each entry and converts to health data format
5. Data is saved as `health_data_linked_list.txt`
6. File is compatible with existing PlotData functions

### Expected Device Response Format
The parser is flexible and can handle multiple formats:
- Key-value pairs: `timestamp:1699276800,temp:28.5,hr:75,sc:1100.5,status:Normal`
- Already formatted: `10:30:15 28.91 76.0 1103.56 Normal`
- Simple comma-separated: `1234567890,28.7,76,1102.3,Normal`

### Output Format
All data is converted to the standard health data format:
```
HH:MM:SS Temperature Heart_Rate Skin_Conductivity Episode
```
Example: `10:30:15 28.91 76.0 1103.56 Normal`

## Usage Instructions

### Step 1: Test Connection
1. Run the main app (`python AppCode.py`)
2. Click "Test Connection" to verify device connectivity
3. Check console output for connection details

### Step 2: Download Linked List Data
1. Click "Download Health Data" in the main app
2. Wait for download to complete
3. Data will be saved to `DataFolder/health_data_linked_list.txt`

### Step 3: View Data
1. Click "Show Graph" in the main app
2. Select the `health_data_linked_list.txt` file
3. Choose which metrics to plot

## Device Protocol Requirements

Your device firmware should respond to the `GET_LINKED_LIST` command by:
1. Sending an acknowledgment (optional)
2. Sending each linked list entry as a line of text
3. Ending with "END_OF_LIST" or closing the connection

## Customization

### Parser Customization
Modify `parse_linked_list_entry()` in `SerialConnection.py` to match your device's exact data format.

### Command Customization
Change `REQUEST_LINKED_LIST` constant to match your device's expected command.

### Timeout Adjustment
Modify the timeout value in `download_linked_list_from_device()` if your device needs more time to respond.

## Testing

### Standalone Test
Run `python test_linked_list_connection.py` to test:
- Parser functionality with sample data
- Device connection
- Linked list download (optional)

### Integration Test
Use the main app buttons to test full integration with the GUI.

## Troubleshooting

### Connection Issues
- Verify device is connected and drivers installed
- Check VID/PID values match your device
- Use "Test Connection" to diagnose issues

### Data Format Issues
- Check console output during download
- Verify device sends data in expected format
- Modify parser if needed

### File Issues
- Ensure DataFolder exists and is writable
- Check file permissions
- Verify PlotData can read the generated file

## Next Steps

1. Test with actual device hardware
2. Adjust parser based on real device data format
3. Fine-tune timeouts and error handling
4. Add progress indicators for long downloads
5. Implement data validation and error recovery