#!/usr/bin/env python3

import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path
from collections import Counter

def analyze_metrics(metrics_file, algo_name):
    
    df = pd.read_csv(metrics_file)
    
    stats = {
        'algorithm': algo_name,
        'total_requests': len(df),
        'mean_response_time': df['response_time_ms'].mean(),
        'median_response_time': df['response_time_ms'].median(),
        'std_response_time': df['response_time_ms'].std(),
        'p95_response_time': df['response_time_ms'].quantile(0.95),
        'p99_response_time': df['response_time_ms'].quantile(0.99),
        'backend_distribution': dict(Counter(df['backend_selected']))
    }
    
    return stats, df

def compare_algorithms(rr_metrics, lrt_metrics, output_dir):
    
    print("\n" + "="*70)
    print("LOAD BALANCING ALGORITHM COMPARISON")
    print("="*70)
    
    rr_stats, rr_df = analyze_metrics(rr_metrics, "Round Robin")
    lrt_stats, lrt_df = analyze_metrics(lrt_metrics, "Least Response Time")
    
    print("\nResponse Time Statistics:")
    print(f"{'Metric':<30} {'Round Robin':<20} {'Least Response Time':<20}")
    print("-" * 70)
    print(f"{'Total Requests':<30} {rr_stats['total_requests']:<20} {lrt_stats['total_requests']:<20}")
    print(f"{'Mean Response Time (ms)':<30} {rr_stats['mean_response_time']:<20.2f} {lrt_stats['mean_response_time']:<20.2f}")
    print(f"{'Median Response Time (ms)':<30} {rr_stats['median_response_time']:<20.2f} {lrt_stats['median_response_time']:<20.2f}")
    print(f"{'Std Dev (ms)':<30} {rr_stats['std_response_time']:<20.2f} {lrt_stats['std_response_time']:<20.2f}")
    print(f"{'P95 Response Time (ms)':<30} {rr_stats['p95_response_time']:<20.2f} {lrt_stats['p95_response_time']:<20.2f}")
    print(f"{'P99 Response Time (ms)':<30} {rr_stats['p99_response_time']:<20.2f} {lrt_stats['p99_response_time']:<20.2f}")
    
    print("\nBackend Distribution:")
    print(f"{'Backend':<15} {'Round Robin':<20} {'Least Response Time':<20}")
    print("-" * 55)
    all_backends = set(rr_stats['backend_distribution'].keys()) | set(lrt_stats['backend_distribution'].keys())
    for backend in sorted(all_backends):
        rr_count = rr_stats['backend_distribution'].get(backend, 0)
        lrt_count = lrt_stats['backend_distribution'].get(backend, 0)
        rr_pct = rr_count / rr_stats['total_requests'] * 100
        lrt_pct = lrt_count / lrt_stats['total_requests'] * 100
        print(f"{'Backend ' + str(backend):<15} {rr_count:>5} ({rr_pct:>5.1f}%)     {lrt_count:>5} ({lrt_pct:>5.1f}%)")
    
    fig, axes = plt.subplots(2, 3, figsize=(18, 10))
    fig.suptitle('Algorithm Comparison: Round Robin vs Least Response Time', fontsize=16)
    
    ax1 = axes[0, 0]
    ax1.hist([rr_df['response_time_ms'], lrt_df['response_time_ms']], 
             bins=30, label=['Round Robin', 'Least Response Time'], alpha=0.7)
    ax1.set_xlabel('Response Time (ms)')
    ax1.set_ylabel('Frequency')
    ax1.set_title('Response Time Distribution')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    ax2 = axes[0, 1]
    backends = sorted(all_backends)
    rr_counts = [rr_stats['backend_distribution'].get(b, 0) for b in backends]
    lrt_counts = [lrt_stats['backend_distribution'].get(b, 0) for b in backends]
    
    x = np.arange(len(backends))
    width = 0.35
    ax2.bar(x - width/2, rr_counts, width, label='Round Robin', alpha=0.7)
    ax2.bar(x + width/2, lrt_counts, width, label='Least Response Time', alpha=0.7)
    ax2.set_xlabel('Backend')
    ax2.set_ylabel('Number of Requests')
    ax2.set_title('Request Distribution Across Backends')
    ax2.set_xticks(x)
    ax2.set_xticklabels([f'B{b}' for b in backends])
    ax2.legend()
    ax2.grid(True, alpha=0.3, axis='y')
    
    ax3 = axes[0, 2]
    ax3.boxplot([rr_df['response_time_ms'], lrt_df['response_time_ms']], 
                labels=['Round Robin', 'LRT'])
    ax3.set_ylabel('Response Time (ms)')
    ax3.set_title('Response Time Comparison')
    ax3.grid(True, alpha=0.3, axis='y')
    
    ax4 = axes[1, 0]
    rr_df_sorted = rr_df.sort_values('timestamp_ms')
    rr_df_sorted['request_num'] = range(len(rr_df_sorted))
    ax4.scatter(rr_df_sorted['request_num'], rr_df_sorted['response_time_ms'], 
               alpha=0.5, s=10)
    ax4.set_xlabel('Request Number')
    ax4.set_ylabel('Response Time (ms)')
    ax4.set_title('Round Robin: Response Time Over Requests')
    ax4.grid(True, alpha=0.3)
    
    ax5 = axes[1, 1]
    lrt_df_sorted = lrt_df.sort_values('timestamp_ms')
    lrt_df_sorted['request_num'] = range(len(lrt_df_sorted))
    ax5.scatter(lrt_df_sorted['request_num'], lrt_df_sorted['response_time_ms'], 
               alpha=0.5, s=10, color='orange')
    ax5.set_xlabel('Request Number')
    ax5.set_ylabel('Response Time (ms)')
    ax5.set_title('Least Response Time: Response Time Over Requests')
    ax5.grid(True, alpha=0.3)
    
    ax6 = axes[1, 2]
    metrics = ['Mean', 'Median', 'P95', 'P99']
    rr_values = [rr_stats['mean_response_time'], rr_stats['median_response_time'],
                 rr_stats['p95_response_time'], rr_stats['p99_response_time']]
    lrt_values = [lrt_stats['mean_response_time'], lrt_stats['median_response_time'],
                  lrt_stats['p95_response_time'], lrt_stats['p99_response_time']]
    
    x = np.arange(len(metrics))
    width = 0.35
    ax6.bar(x - width/2, rr_values, width, label='Round Robin', alpha=0.7)
    ax6.bar(x + width/2, lrt_values, width, label='Least Response Time', alpha=0.7)
    ax6.set_ylabel('Response Time (ms)')
    ax6.set_title('Response Time Metrics Comparison')
    ax6.set_xticks(x)
    ax6.set_xticklabels(metrics)
    ax6.legend()
    ax6.grid(True, alpha=0.3, axis='y')
    
    plt.tight_layout()
    
    output_file = Path(output_dir) / "algorithm_comparison.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"\nComparison plot saved to: {output_file}")
    
    plt.show()

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 compare_algorithms.py <rr_metrics.log> <lrt_metrics.log>")
        print("\nExample:")
        print("  python3 compare_algorithms.py results_lb/exp1_rr_c8_metrics.log results_lb/exp1_lrt_c8_metrics.log")
        sys.exit(1)
    
    rr_metrics = sys.argv[1]
    lrt_metrics = sys.argv[2]
    
    if not Path(rr_metrics).exists():
        print(f"Error: File not found: {rr_metrics}")
        sys.exit(1)
    
    if not Path(lrt_metrics).exists():
        print(f"Error: File not found: {lrt_metrics}")
        sys.exit(1)
    
    output_dir = Path(rr_metrics).parent
    
    compare_algorithms(rr_metrics, lrt_metrics, output_dir)

if __name__ == "__main__":
    main()