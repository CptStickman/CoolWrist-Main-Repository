#!/usr/bin/env python3

import matplotlib.pyplot as plt
import pandas as pd
import tkinter as tk
from tkinter import ttk, messagebox
import os
import glob
import re

def is_valid_health_data_file(filepath):
    """
    Check if a file has the correct health data format.
    Expected format: Time Heart_Rate Skin_Conductivity Episode
    Example: 00:00:00 76.0 1103.56 Normal
    """
    try:
        with open(filepath, 'r') as file:
            first_line = file.readline().strip()
            
        # Check if the line is empty
        if not first_line:
            return False
            
        # Split the line into components
        parts = first_line.split()
        
        # Should have exactly 4 parts: Time, Heart_Rate, Skin_Conductivity, Episode
        if len(parts) != 4:
            return False
            
        # Check if first part matches time format (HH:MM:SS)
        time_pattern = r'^\d{2}:\d{2}:\d{2}$'
        if not re.match(time_pattern, parts[0]):
            return False
            
        # Check if second and third parts are valid numbers (heart rate and skin conductivity)
        try:
            float(parts[1])  # Heart Rate
            float(parts[2])  # Skin Conductivity
        except ValueError:
            return False
            
        # Fourth part should be episode status (typically "Normal" or similar text)
        # We don't need to validate the exact value, just that it exists
        
        return True
        
    except Exception:
        return False

def plot_specific_file(filename):
    """Create an overview plot of health data from a specific file"""
    try:
        # Load the data
        filepath = os.path.join("DataFolder", filename)
        data = pd.read_csv(filepath, sep=' ', header=None, 
                          names=['Time', 'Heart_Rate', 'Skin_Conductivity', 'Episode'])
        
        # Create a simple plot
        fig, axes = plt.subplots(2, 1, figsize=(12, 6))
        fig.suptitle(f'Health Data Overview - {filename}', fontsize=14, fontweight='bold')
        
        # Plot each metric
        axes[0].plot(data['Heart_Rate'], color='blue', linewidth=0.8)
        axes[0].set_title('Heart Rate (BPM)')
        axes[0].set_xlim(0, len(data) - 1)
        axes[0].grid(True, alpha=0.3)
        
        axes[1].plot(data['Skin_Conductivity'], color='green', linewidth=0.8)
        axes[1].set_title('Skin Conductivity (mV)')
        axes[1].set_xlabel('Time (seconds)')
        axes[1].set_xlim(0, len(data) - 1)
        axes[1].grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.show()
        
        print(f"Successfully plotted {len(data)} data points from {filename}!")
        
    except Exception as e:
        messagebox.showerror("Error", f"Error plotting {filename}: {e}")

def get_available_data_files():
    """Get list of available valid health data files in DataFolder"""
    data_folder = "DataFolder"
    if not os.path.exists(data_folder):
        return []
    
    # Look for common data file extensions
    file_patterns = ["*.txt", "*.csv", "*.dat"]
    files = []
    
    for pattern in file_patterns:
        files.extend(glob.glob(os.path.join(data_folder, pattern)))
    
    # Filter files to only include valid health data files
    valid_files = []
    for filepath in files:
        if is_valid_health_data_file(filepath):
            valid_files.append(os.path.basename(filepath))
    
    return valid_files

def plot_Data():
    """Backward compatibility function - plots the first available valid file"""
    available_files = get_available_data_files()
    if available_files:
        plot_specific_file(available_files[0])
    else:
        messagebox.showerror("Error", "No valid health data files found in DataFolder!\n\n" +
                           "Files must have the format:\nTime Heart_Rate Skin_Conductivity Episode\n" +
                           "Example: 00:00:00 76.0 1103.56 Normal")
        
def create_file_selection_interface(root, back_callback):
    """Create the file selection interface in the provided root window"""
    # Get available files
    available_files = get_available_data_files()
    
    if not available_files:
        messagebox.showerror("Error", "No valid health data files found in DataFolder!\n\n" +
                           "Files must have the format:\nTime Heart_Rate Skin_Conductivity Episode\n" +
                           "Example: 00:00:00 76.0 1103.56 Normal")
        back_callback()
        return None, None
    
    # Title label
    title_label = tk.Label(root, text="Choose a data file to plot:", 
                          font=("Arial", 12, "bold"), bg="#054067", fg="#ECF0F1")
    title_label.pack(pady=30)
    
    # Dropdown menu
    selected_file = tk.StringVar(value=available_files[0])
    dropdown = ttk.Combobox(root, textvariable=selected_file, 
                           values=available_files, state="readonly", width=40)
    dropdown.pack(pady=20)
    
    # Button frame
    button_frame = tk.Frame(root, bg="#054067")
    button_frame.pack(pady=30)
    
    def plot_selected_file():
        filename = selected_file.get()
        plot_specific_file(filename)
    
    # Buttons
    plot_btn = tk.Button(button_frame, text="Plot Graph", command=plot_selected_file,
                        bg="#3498DB", fg="white", font=("Arial", 10, "bold"),
                        padx=20, pady=8, width=15)
    plot_btn.pack(side=tk.TOP, pady=10)
    
    back_btn = tk.Button(button_frame, text="← Back to Main Menu", command=back_callback,
                        bg="#E74C3C", fg="white", font=("Arial", 10, "bold"),
                        padx=20, pady=8, width=15)
    back_btn.pack(side=tk.TOP, pady=5)
    
    return [title_label, dropdown, button_frame], selected_file

if __name__ == "__main__":
    plot_Data()