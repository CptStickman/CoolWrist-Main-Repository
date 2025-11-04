#!/usr/bin/env python3

import matplotlib.pyplot as plt
import pandas as pd
import tkinter as tk
from tkinter import ttk, messagebox
import os
import glob

def plot_specific_file(filename):
    """Create an overview plot of health data from a specific file"""
    try:
        # Load the data
        filepath = os.path.join("DataFolder", filename)
        data = pd.read_csv(filepath, sep=' ', header=None, 
                          names=['Time', 'Temperature', 'Heart_Rate', 'Skin_Conductivity', 'Episode'])
        
        # Create a simple plot
        fig, axes = plt.subplots(3, 1, figsize=(12, 8))
        fig.suptitle(f'Health Data Overview - {filename}', fontsize=14, fontweight='bold')
        
        # Plot each metric
        axes[0].plot(data['Temperature'], color='red', linewidth=0.8)
        axes[0].set_title('Temperature (°C)')
        axes[0].grid(True, alpha=0.3)
        
        axes[1].plot(data['Heart_Rate'], color='blue', linewidth=0.8)
        axes[1].set_title('Heart Rate (BPM)')
        axes[1].grid(True, alpha=0.3)
        
        axes[2].plot(data['Skin_Conductivity'], color='green', linewidth=0.8)
        axes[2].set_title('Skin Conductivity (mV)')
        axes[2].set_xlabel('Time (seconds)')
        axes[2].grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.show()
        
        print(f"Successfully plotted {len(data)} data points from {filename}!")
        
    except Exception as e:
        messagebox.showerror("Error", f"Error plotting {filename}: {e}")

def get_available_data_files():
    """Get list of available data files in DataFolder"""
    data_folder = "DataFolder"
    if not os.path.exists(data_folder):
        return []
    
    # Look for common data file extensions
    file_patterns = ["*.txt", "*.csv", "*.dat"]
    files = []
    
    for pattern in file_patterns:
        files.extend(glob.glob(os.path.join(data_folder, pattern)))
    
    # Return just the filenames, not full paths
    return [os.path.basename(f) for f in files]

def plot_Data():
    """Backward compatibility function - plots the first available file"""
    available_files = get_available_data_files()
    if available_files:
        plot_specific_file(available_files[0])
    else:
        messagebox.showerror("Error", "No data files found in DataFolder!")

def create_file_selection_interface(root, back_callback):
    """Create the file selection interface in the provided root window"""
    # Get available files
    available_files = get_available_data_files()
    
    if not available_files:
        messagebox.showerror("Error", "No data files found in DataFolder!")
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