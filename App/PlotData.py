#!/usr/bin/env python3

import matplotlib.pyplot as plt
import pandas as pd

def plot_Data():
    """Create an overview plot of health data"""
    try:
        # Load the data
        data = pd.read_csv("DataFolder/health_data.txt", sep=' ', header=None, 
                          names=['Time', 'Temperature', 'Heart_Rate', 'Skin_Conductivity'])
        
        # Create a simple plot
        fig, axes = plt.subplots(3, 1, figsize=(12, 8))
        fig.suptitle('Health Data Overview', fontsize=14, fontweight='bold')
        
        # Plot each metric
        axes[0].plot(data['Temperature'], color='red', linewidth=0.8)
        axes[0].set_title('Temperature (°C)')
        axes[0].grid(True, alpha=0.3)
        
        axes[1].plot(data['Heart_Rate'], color='blue', linewidth=0.8)
        axes[1].set_title('Heart Rate (BPM)')
        axes[1].grid(True, alpha=0.3)
        
        axes[2].plot(data['Skin_Conductivity'], color='green', linewidth=0.8)
        axes[2].set_title('Skin Conductivity')
        axes[2].set_xlabel('Time (seconds)')
        axes[2].grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.show()
        
        print(f"Successfully plotted {len(data)} data points!")
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    plot_Data()