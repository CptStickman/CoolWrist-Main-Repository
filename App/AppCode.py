import tkinter as tk
from tkinter import messagebox
from SerialConnection import download_file_from_device, find_uart_device

import PlotData

DataFolder = "DataFolder/TestFile.txt"

def handle_download():
    port = find_uart_device(vid=0x0483, pid=0x374E)  # Use STM32 VID/PID
    if not port:
        messagebox.showerror("Error", "No STM32 device found with specified VID/PID.")
        return
    path = download_file_from_device(port)
    if path:
        messagebox.showinfo("Success", f"File downloaded to {path}")
    else:
        messagebox.showerror("Error", "Failed to download file.")

def read_from_file():
    try:
        with open(DataFolder, "r") as f:
            content = f.read()
        messagebox.showinfo("File Content", content)
    except FileNotFoundError:
        messagebox.showerror("Error", "File not found.")



def main():
    root = tk.Tk()
    root.title("CoolWrist App")
    root.geometry("300x400")  # Width x Height
    root.configure(bg="#054067")  # Dark blue-gray background
   
    # Store references to widgets and their pack configurations
    main_widgets = []
    selection_widgets = []

    def show_main_menu():
        """Show the main menu interface"""
        # Hide selection widgets
        for widget in selection_widgets:
            widget.pack_forget()
        selection_widgets.clear()
        
        # Show main widgets with their original configurations
        for widget, pack_config in main_widgets:
            widget.pack(**pack_config)

    def show_graph_selection():
        """Transform window to show graph selection interface"""
        # Hide main menu widgets
        for widget, _ in main_widgets:
            widget.pack_forget()
        
        # Create and show selection interface
        widgets, _ = PlotData.create_file_selection_interface(root, show_main_menu)
        if widgets:
            selection_widgets.extend(widgets)

    def show_message():
        messagebox.showinfo("Information", "This is a sample message.")

    # Create main menu widgets and store with their pack configurations
    label = tk.Label(root, text="Welcome to CoolWrist!", font=("Arial", 14), 
                     bg="#054067", fg="#ECF0F1")  # White text on dark background
    label.pack(pady=40)
    main_widgets.append((label, {'pady': 40}))

    # button = tk.Button(root, text="Click Me", command=show_message)
    # button.pack(pady=20)
    # main_widgets.append((button, {'pady': 20}))

    # read_button = tk.Button(root, text="Read from File", command=read_from_file)
    # read_button.pack(pady=5)
    # main_widgets.append((read_button, {'pady': 5}))

    download_btn = tk.Button(root, text="Download File", command=handle_download,
                            bg="#3498DB", fg="white", font=("Arial", 10, "bold"),
                            padx=20, pady=8, width=15)
    download_btn.pack(pady=5)
    main_widgets.append((download_btn, {'pady': 5}))

    graph_btn = tk.Button(root, text="Show Graph", command=show_graph_selection,
                         bg="#3498DB", fg="white", font=("Arial", 10, "bold"),
                         padx=20, pady=8, width=15)
    graph_btn.pack(pady=5)
    main_widgets.append((graph_btn, {'pady': 5}))

    exit_btn = tk.Button(root, text="Exit", command=root.destroy,
                        bg="#E74C3C", fg="white", font=("Arial", 10, "bold"),
                        padx=20, pady=8, width=15)
    exit_btn.pack(pady=5)
    main_widgets.append((exit_btn, {'pady': 5}))

    root.mainloop()

if __name__ == "__main__":
    main()