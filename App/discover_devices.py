#!/usr/bin/env python3
"""
Quick script to discover VID/PID of connected serial devices
"""
from SerialConnection import list_all_serial_devices

if __name__ == "__main__":
    list_all_serial_devices()