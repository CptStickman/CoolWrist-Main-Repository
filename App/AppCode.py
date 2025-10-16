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

    def show_message():
        messagebox.showinfo("Information", "This is a sample message.")

    label = tk.Label(root, text="Welcome to CoolWrist!", font=("Arial", 14))
    label.pack(pady=40)

    # button = tk.Button(root, text="Click Me", command=show_message)
    # button.pack(pady=20)

    # read_button = tk.Button(root, text="Read from File", command=read_from_file)
    # read_button.pack(pady=5)

    download_btn = tk.Button(root, text="Download File", command=handle_download)
    download_btn.pack(pady=5)

    graph_btn = tk.Button(root, text="Show Graph", command=PlotData.plot_Data)
    graph_btn.pack(pady=5)

    exit_btn = tk.Button(root, text="Exit", command=root.destroy)
    exit_btn.pack(pady=5)

    root.mainloop()

if __name__ == "__main__":
    main()