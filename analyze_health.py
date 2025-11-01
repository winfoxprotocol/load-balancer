#!/usr/bin/env python3

import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

def analyze_health_log(log_file):
    
    print(f"\n{'='*60}")
    print(f"Health Check Analysis: {log_file}")
    print(f"{'='*60}\n")
    
    df = pd.read_csv(log_file)
    
    df['time_sec'] = (df['timestamp_ms'] - df['timestamp_ms'].min()) / 1000.0
    
    print("Overall Statistics:")
    print(f"  Duration: {df['time_sec'].max():.1f} seconds")
    print(f"  Total checks: {len(df)}")
    print(f"  Successful checks: {(df['status'] == 'OK').sum()}")
    print(f"  Failed checks: {(df['status'] == 'FAIL').sum()}")
    print(f"  Success rate: {(df['status'] == 'OK').sum() / len(df) * 100:.1f}%")
    
    print("\nPer-Backend Statistics:")
    print(f"{'Backend':<10} {'Port':<8} {'Checks':<10} {'Success':<10} {'Fail':<10} {'Avg RTT (ms)':<15}")
    print("-" * 70)
    
    for backend_id in sorted(df['backend_id'].unique()):
        backend_df = df[df['backend_id'] == backend_id]
        success_df = backend_df[backend_df['status'] == 'OK']
        fail_count = len(backend_df[backend_df['status'] == 'FAIL'])
        avg_rtt = success_df['rtt_ms'].mean() if len(success_df) > 0 else 0
        port = backend_df['port'].iloc[0]
        
        print(f"{backend_id:<10} {port:<8} {len(backend_df):<10} "
              f"{len(success_df):<10} {fail_count:<10} {avg_rtt:<15.2f}")
    
    success_df = df[df['status'] == 'OK']
    if len(success_df) > 0:
        print("\nRTT Statistics (successful checks only):")
        print(f"  Mean RTT: {success_df['rtt_ms'].mean():.2f} ms")
        print(f"  Median RTT: {success_df['rtt_ms'].median():.2f} ms")
        print(f"  Min RTT: {success_df['rtt_ms'].min():.2f} ms")
        print(f"  Max RTT: {success_df['rtt_ms'].max():.2f} ms")
        print(f"  Std Dev: {success_df['rtt_ms'].std():.2f} ms")
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle(f'Health Check Analysis: {Path(log_file).name}', fontsize=16)
    
    ax1 = axes[0, 0]
    for backend_id in sorted(df['backend_id'].unique()):
        backend_df = df[(df['backend_id'] == backend_id) & (df['status'] == 'OK')]
        if len(backend_df) > 0:
            ax1.plot(backend_df['time_sec'], backend_df['rtt_ms'], 
                    marker='o', label=f'Backend {backend_id}', alpha=0.7)
    ax1.set_xlabel('Time (seconds)')
    ax1.set_ylabel('RTT (ms)')
    ax1.set_title('RTT Over Time')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    ax2 = axes[0, 1]
    rtt_data = []
    labels = []
    for backend_id in sorted(df['backend_id'].unique()):
        backend_df = df[(df['backend_id'] == backend_id) & (df['status'] == 'OK')]
        if len(backend_df) > 0:
            rtt_data.append(backend_df['rtt_ms'])
            labels.append(f'B{backend_id}')
    if rtt_data:
        ax2.boxplot(rtt_data, labels=labels)
        ax2.set_ylabel('RTT (ms)')
        ax2.set_title('RTT Distribution by Backend')
        ax2.grid(True, alpha=0.3, axis='y')
    
    ax3 = axes[1, 0]
    backend_ids = sorted(df['backend_id'].unique())
    success_rates = []
    for backend_id in backend_ids:
        backend_df = df[df['backend_id'] == backend_id]
        success_rate = (backend_df['status'] == 'OK').sum() / len(backend_df) * 100
        success_rates.append(success_rate)
    
    colors = ['green' if rate == 100 else 'orange' if rate > 90 else 'red' 
              for rate in success_rates]
    ax3.bar([f'B{id}' for id in backend_ids], success_rates, color=colors, alpha=0.7)
    ax3.set_ylabel('Success Rate (%)')
    ax3.set_title('Health Check Success Rate by Backend')
    ax3.set_ylim([0, 105])
    ax3.grid(True, alpha=0.3, axis='y')
    
    ax4 = axes[1, 1]
    avg_rtts = []
    for backend_id in backend_ids:
        backend_df = df[(df['backend_id'] == backend_id) & (df['status'] == 'OK')]
        avg_rtt = backend_df['rtt_ms'].mean() if len(backend_df) > 0 else 0
        avg_rtts.append(avg_rtt)
    
    ax4.bar([f'B{id}' for id in backend_ids], avg_rtts, alpha=0.7, color='skyblue')
    ax4.set_ylabel('Average RTT (ms)')
    ax4.set_title('Average RTT by Backend')
    ax4.grid(True, alpha=0.3, axis='y')
    
    plt.tight_layout()
    
    output_file = log_file.replace('.log', '_analysis.png')
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"\nPlot saved to: {output_file}")
    
    plt.show()

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 analyze_health.py <health_check_log_file>")
        print("\nExample:")
        print("  python3 analyze_health.py results_lb/exp1_rr_c8_health.log")
        sys.exit(1)
    
    log_file = sys.argv[1]
    
    if not Path(log_file).exists():
        print(f"Error: File not found: {log_file}")
        sys.exit(1)
    
    analyze_health_log(log_file)

if __name__ == "__main__":
    main()