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
    Expected format: Time Skin_Conductivity Episode (Heart_Rate removed due to hardware failure)
    Example: 00:00:00 2605.00 0
    """
    try:
        with open(filepath, 'r') as file:
            lines = file.readlines()
            
        # Find the first valid data line (skip debug messages)
        valid_line = None
        for line in lines:
            line = line.strip()
            if not line:
                continue
            
            # Skip debug/status messages
            if any(debug_word in line.lower() for debug_word in 
                   ['isr', 'received', 'sending', 'command:', 'entries']):
                continue
                
            # Check if this looks like a data line
            parts = line.split()
            if len(parts) >= 3 and ':' in parts[0]:
                valid_line = line
                break
        
        if not valid_line:
            return False
            
        # Split the line into components
        parts = valid_line.split()
        
        # Should have exactly 3 parts: Time, Skin_Conductivity, Episode (Heart_Rate removed)
        if len(parts) != 3:
            return False
            
        # Check if first part matches time format (HH:MM:SS)
        time_pattern = r'^\d{2}:\d{2}:\d{2}$'
        if not re.match(time_pattern, parts[0]):
            return False
            
        # Check if second part is valid number (skin conductivity)
        try:
            float(parts[1])  # Skin Conductivity
        except ValueError:
            return False
            
        # Third part should be episode status (typically "0", "1", or similar)
        # We don't need to validate the exact value, just that it exists
        
        return True
        
    except Exception:
        return False

def clean_data_file(filepath):
    """
    Clean the data file by removing debug messages and keeping only valid data lines.
    Returns a list of clean data lines.
    """
    clean_lines = []
    try:
        with open(filepath, 'r') as file:
            lines = file.readlines()
        
        for line in lines:
            line = line.strip()
            if not line:
                continue
                
            # Skip debug/status messages and device diagnostic info
            if any(debug_word in line.lower() for debug_word in 
                   ['isr', 'received', 'sending', 'command:', 'entries', 'condition']):
                continue
            
            # Skip lines that are just numbers (like "0")
            if line.isdigit():
                continue
                
            # Check if this looks like a data line
            parts = line.split()
            if len(parts) >= 3 and ':' in parts[0]:
                # Additional validation to filter out device diagnostic data
                try:
                    # Check if it's a valid time format
                    time_pattern = r'^\d{2}:\d{2}:\d{2}$'
                    if not re.match(time_pattern, parts[0]):
                        continue
                    
                    # Parse skin conductivity value
                    skin_conductivity = float(parts[1])
                    
                    # Skip lines with 0.0 skin conductivity as they're likely diagnostic
                    if skin_conductivity == 0.0:
                        continue
                    
                    # Skip if the third part contains text mixed with numbers (like "286.000000condition")
                    if not parts[2].replace('.', '').replace('-', '').isalnum():
                        continue
                    
                    # Take only first 3 parts (Time, Skin_Conductivity, Episode)
                    clean_line = f"{parts[0]} {parts[1]} {parts[2]}"
                    clean_lines.append(clean_line)
                    
                except (ValueError, IndexError):
                    # Skip lines that can't be properly parsed
                    continue
                
    except Exception as e:
        print(f"Error cleaning data file: {e}")
        
    return clean_lines

def plot_specific_file(filename):
    """Create an overview plot of health data from a specific file"""
    try:
        # Load and clean the data
        filepath = os.path.join("DataFolder", filename)
        
        # Clean the data first
        clean_lines = clean_data_file(filepath)
        if not clean_lines:
            messagebox.showerror("Error", f"No valid data found in {filename}")
            return
            
        # Parse the clean data
        data_rows = []
        for line in clean_lines:
            parts = line.split()
            if len(parts) == 3:
                time_str, skin_conductivity, episode = parts
                data_rows.append({
                    'Time': time_str,
                    'Skin_Conductivity': float(skin_conductivity),
                    'Episode': int(episode)
                })
        
        if not data_rows:
            messagebox.showerror("Error", f"No valid data rows found in {filename}")
            return
            
        # Convert to DataFrame
        import pandas as pd
        data = pd.DataFrame(data_rows)
        
        # Create a simple plot - only one subplot now since heart rate is removed
        fig, axes = plt.subplots(1, 1, figsize=(12, 6))
        fig.suptitle(f'Health Data Overview - {filename}', fontsize=14, fontweight='bold')
        
        # Plot skin conductivity only (heart rate commented out due to hardware failure)
        # axes[0].plot(data['Heart_Rate'], color='red', linewidth=0.8)
        # axes[0].set_title('Heart Rate (BPM)')
        # axes[0].set_xlim(0, len(data) - 1)
        # axes[0].grid(True, alpha=0.3)
        
        axes.plot(data['Skin_Conductivity'], color='green', linewidth=0.8)
        axes.set_title('Skin Conductivity (mV)')
        axes.set_xlabel('Time (seconds)')
        axes.set_xlim(0, len(data) - 1)
        axes.grid(True, alpha=0.3)

         # Highlight episode areas
        episode_count = 0
        i = 0
        while i < len(data):
            if data['Episode'].iloc[i] == 1:  # Start of an episode
                episode_start = i
                # Find the end of this episode
                while i < len(data) and data['Episode'].iloc[i] == 1:
                    i += 1
                episode_end = i - 1
                
                # Highlight the episode area
                axes.axvspan(episode_start, episode_end, 
                           alpha=0.3, color='red', 
                           label='Episode' if episode_count == 0 else "")
                episode_count += 1
            else:
                i += 1
        
        # Add some statistics
        avg_conductivity = data['Skin_Conductivity'].mean()
        axes.axhline(y=avg_conductivity, color='red', linestyle='--', alpha=0.7, 
                    label=f'Average: {avg_conductivity:.1f} mV')
        
        # Add legend and episode count info
        axes.legend(loc='upper right')
        if episode_count > 0:
            axes.text(0.98, 0.85, f'Episodes detected: {episode_count}', 
                     transform=axes.transAxes, fontsize=10, 
                     verticalalignment='top', horizontalalignment='right',
                     bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
        
        plt.tight_layout()
        plt.show()
        
        print(f"Successfully plotted {len(data)} data points from {filename}!")
        print(f"Average Skin Conductivity: {avg_conductivity:.2f} mV")
        if episode_count > 0:
            print(f"Episodes detected: {episode_count}")
        else:
            print("No episodes detected in this data.")
        

    except Exception as e:
        messagebox.showerror("Error", f"Error plotting {filename}: {e}")
        print(f"Detailed error: {e}")

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
                           "Files must have the format:\nTime Skin_Conductivity Episode\n" +
                           "Example: 00:00:00 2605.00 0\n")

def create_file_selection_interface(root, back_callback):
    """Create the file selection interface in the provided root window"""
    # Get available files
    available_files = get_available_data_files()
    
    if not available_files:
        messagebox.showerror("Error", "No valid health data files found in DataFolder!\n\n" +
                           "Files must have the format:\nTime Skin_Conductivity Episode\n" +
                           "Example: 00:00:00 1103.56 Normal")
        back_callback()
        return None, None
    
    # Store widgets for this interface
    selection_widgets = []
    combine_widgets = []

    # Title label
    title_label = tk.Label(root, text="Choose a data file to plot:", 
                          font=("Arial", 12, "bold"), bg="#054067", fg="#ECF0F1")
    title_label.pack(pady=30)
    selection_widgets.append((title_label, {'pady': 30}))
    
    # Dropdown menu
    selected_file = tk.StringVar(value=available_files[0])
    dropdown = ttk.Combobox(root, textvariable=selected_file, 
                           values=available_files, state="readonly", width=40)
    dropdown.pack(pady=20)
    selection_widgets.append((dropdown, {'pady': 20}))

    # Button frame
    button_frame = tk.Frame(root, bg="#054067")
    button_frame.pack(pady=30)
    selection_widgets.append((button_frame, {'pady': 30}))

    def plot_selected_file():
        filename = selected_file.get()
        plot_specific_file(filename)

    def show_combine_interface():
        """Hide selection interface and show combine interface"""
        # Hide selection widgets
        for widget, _ in selection_widgets:
            widget.pack_forget()
        
        # Create and show combine interface
        combine_widgets_list = create_combine_widgets(root, available_files, show_selection_interface)
        combine_widgets.extend(combine_widgets_list)
    
    def show_selection_interface():
        """Hide combine interface and show selection interface"""
        # Hide combine widgets
        for widget, _ in combine_widgets:
            widget.pack_forget()
        combine_widgets.clear()
        
        # Show selection widgets
        for widget, pack_config in selection_widgets:
            widget.pack(**pack_config)
    
    # Buttons
    plot_btn = tk.Button(button_frame, text="Plot Graph", command=plot_selected_file,
                        bg="#3498DB", fg="white", font=("Arial", 10, "bold"),
                        padx=20, pady=8, width=15)
    plot_btn.pack(side=tk.TOP, pady=10)

    combine_btn = tk.Button(button_frame, text="Combine Files", command=show_combine_interface,
                        bg="#3498DB", fg="white", font=("Arial", 10, "bold"),
                        padx=20, pady=8, width=15)
    combine_btn.pack(side=tk.TOP, pady=10)
    
    back_btn = tk.Button(button_frame, text="← Back to Main Menu", command=back_callback,
                        bg="#E74C3C", fg="white", font=("Arial", 10, "bold"),
                        padx=20, pady=8, width=15)
    back_btn.pack(side=tk.TOP, pady=5)
    
    return [title_label, dropdown, button_frame], selected_file

def combine_Data_Files(file_list, output_filename=None):
    """
    Combine multiple health data files into one with continuous time progression.
    The second file's timestamps continue from where the first file ended.
    """
    if len(file_list) < 2:
        messagebox.showerror("Error", "Please select at least 2 files to combine.")
        return None
    
    combined_lines = []
    total_seconds = 0  # Track cumulative time
    
    for i, filename in enumerate(file_list):
        filepath = os.path.join("DataFolder", filename)
        clean_lines = clean_data_file(filepath)
        
        if not clean_lines:
            print(f"Warning: No valid data found in {filename}")
            continue
        
        # For the first file, use original timestamps
        if i == 0:
            combined_lines.extend(clean_lines)
            
            # Find the last timestamp to continue from
            if clean_lines:
                last_line = clean_lines[-1]
                last_time_str = last_line.split()[0]
                # Convert HH:MM:SS to total seconds
                time_parts = last_time_str.split(':')
                total_seconds = (int(time_parts[0]) * 3600 + 
                               int(time_parts[1]) * 60 + 
                               int(time_parts[2])) + 1  # Start next file at +1 second
        else:
            # For subsequent files, adjust timestamps to continue from previous
            for line in clean_lines:
                parts = line.split()
                if len(parts) == 3:
                    # Parse original timestamp
                    original_time = parts[0]
                    original_parts = original_time.split(':')
                    original_seconds = (int(original_parts[0]) * 3600 + 
                                      int(original_parts[1]) * 60 + 
                                      int(original_parts[2]))
                    
                    # Calculate new continuous timestamp
                    new_total_seconds = total_seconds + original_seconds
                    new_hours = new_total_seconds // 3600
                    new_minutes = (new_total_seconds % 3600) // 60
                    new_secs = new_total_seconds % 60
                    
                    new_timestamp = f"{new_hours:02d}:{new_minutes:02d}:{new_secs:02d}"
                    
                    # Create new line with adjusted timestamp
                    new_line = f"{new_timestamp} {parts[1]} {parts[2]}"
                    combined_lines.append(new_line)
            
            # Update total_seconds for the next file
            if clean_lines:
                last_original_line = clean_lines[-1]
                last_original_time = last_original_line.split()[0]
                last_original_parts = last_original_time.split(':')
                last_original_seconds = (int(last_original_parts[0]) * 3600 + 
                                       int(last_original_parts[1]) * 60 + 
                                       int(last_original_parts[2]))
                total_seconds += last_original_seconds + 1
    
    if not combined_lines:
        messagebox.showerror("Error", "No valid data found in any of the selected files.")
        return None
    
    # Generate output filename if not provided
    if output_filename is None:
        import datetime
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        output_filename = f"Combined_Health_Data_C_{timestamp}.txt"
    
    # Write combined data to output file
    output_path = os.path.join("DataFolder", output_filename)
    try:
        with open(output_path, 'w') as outfile:
            for line in combined_lines:
                outfile.write(line + '\n')
        
        success_msg = (f"Successfully combined {len(file_list)} files into {output_filename}\n"
                      f"Total data points: {len(combined_lines)}\n"
                      f"Files combined: {', '.join(file_list)}")
        print(success_msg)
        messagebox.showinfo("Success", success_msg)
        return output_path
        
    except Exception as e:
        error_msg = f"Error writing combined data file: {e}"
        print(error_msg)
        messagebox.showerror("Error", error_msg)
        return None
    
def create_combine_widgets(root, available_files, back_callback):
    """Create the combine interface widgets"""
    if len(available_files) < 2:
        messagebox.showerror("Error", "Need at least 2 valid health data files to combine!")
        back_callback()
        return []
    
    combine_widgets = []
    
    # Title label
    title_label = tk.Label(root, text="Select files to combine (in order):", 
                          font=("Arial", 12, "bold"), bg="#054067", fg="#ECF0F1")
    title_label.pack(pady=20)
    combine_widgets.append((title_label, {'pady': 20}))
    
    # Instructions
    instruction_label = tk.Label(root, text="Hold Ctrl to select multiple files. Order matters for time continuation.", 
                                font=("Arial", 9), bg="#054067", fg="#BDC3C7")
    instruction_label.pack(pady=5)
    combine_widgets.append((instruction_label, {'pady': 5}))
    
    # Listbox for multiple selection
    listbox_frame = tk.Frame(root, bg="#054067")
    listbox_frame.pack(pady=20)
    combine_widgets.append((listbox_frame, {'pady': 20}))
    
    scrollbar = tk.Scrollbar(listbox_frame)
    scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
    
    file_listbox = tk.Listbox(listbox_frame, selectmode=tk.MULTIPLE, 
                             yscrollcommand=scrollbar.set, width=60, height=8)
    file_listbox.pack(side=tk.LEFT, fill=tk.BOTH)
    scrollbar.config(command=file_listbox.yview)
    
    # Populate listbox
    for file in available_files:
        file_listbox.insert(tk.END, file)
    
    # Button frame
    button_frame = tk.Frame(root, bg="#054067")
    button_frame.pack(pady=30)
    combine_widgets.append((button_frame, {'pady': 30}))
    
    def combine_selected_files():
        selected_indices = file_listbox.curselection()
        if len(selected_indices) < 2:
            messagebox.showerror("Error", "Please select at least 2 files to combine.")
            return
        
        selected_files = [available_files[i] for i in selected_indices]
        result = combine_Data_Files(selected_files)
        if result:
            messagebox.showinfo("Success", f"Files combined successfully!\nOutput: {os.path.basename(result)}")
    
    # Buttons
    combine_btn = tk.Button(button_frame, text="Combine Files", command=combine_selected_files,
                           bg="#3498DB", fg="white", font=("Arial", 10, "bold"),
                           padx=20, pady=8, width=15)
    combine_btn.pack(side=tk.TOP, pady=10)
    
    back_btn = tk.Button(button_frame, text="← Back to File Selection", command=back_callback,
                        bg="#E74C3C", fg="white", font=("Arial", 10, "bold"),
                        padx=20, pady=8, width=15)
    back_btn.pack(side=tk.TOP, pady=5)
    
    return combine_widgets

if __name__ == "__main__":
    plot_Data()