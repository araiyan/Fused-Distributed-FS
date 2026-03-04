#!/usr/bin/env python3
"""
Generate graphs from FUSE benchmark results
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys

def load_results(csv_file='benchmark_results.csv'):
    """Load benchmark results from CSV"""
    try:
        df = pd.read_csv(csv_file)
        print(f"Loaded {len(df)} results from {csv_file}")
        return df
    except FileNotFoundError:
        print(f"Error: {csv_file} not found. Run benchmark.sh first.")
        sys.exit(1)

def plot_throughput_comparison(df):
    """Compare throughput across filesystems"""
    # Filter tests that have throughput data
    throughput_tests = df[df['throughput_mbps'] != 'N/A'].copy()
    throughput_tests['throughput_mbps'] = pd.to_numeric(throughput_tests['throughput_mbps'])
    
    # Pivot data for plotting
    pivot = throughput_tests.pivot(index='test', columns='filesystem', values='throughput_mbps')
    
    # Create bar chart
    ax = pivot.plot(kind='bar', figsize=(12, 6), rot=45)
    ax.set_ylabel('Throughput (MB/s)')
    ax.set_xlabel('Test')
    ax.set_title('Throughput Comparison: FUSE vs tmpfs')
    ax.legend(title='Filesystem')
    ax.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('throughput_comparison.png', dpi=300)
    print("Saved: throughput_comparison.png")
    plt.close()

def plot_latency_comparison(df):
    """Compare latency across filesystems"""
    # Pivot data
    pivot = df.pivot(index='test', columns='filesystem', values='avg_latency_ms')
    
    # Create bar chart
    ax = pivot.plot(kind='bar', figsize=(12, 6), rot=45, color=['#FF6B6B', '#4ECDC4'])
    ax.set_ylabel('Average Latency (ms)')
    ax.set_xlabel('Test')
    ax.set_title('Latency Comparison: FUSE vs tmpfs')
    ax.legend(title='Filesystem')
    ax.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('latency_comparison.png', dpi=300)
    print("Saved: latency_comparison.png")
    plt.close()

def plot_overhead_percentage(df):
    """Calculate and plot FUSE overhead vs tmpfs"""
    # Filter throughput tests
    throughput_tests = df[df['throughput_mbps'] != 'N/A'].copy()
    throughput_tests['throughput_mbps'] = pd.to_numeric(throughput_tests['throughput_mbps'])
    
    # Pivot and calculate overhead
    pivot = throughput_tests.pivot(index='test', columns='filesystem', values='throughput_mbps')
    pivot['overhead_percent'] = ((pivot['tmpfs'] - pivot['fuse']) / pivot['tmpfs'] * 100)
    
    # Create bar chart
    ax = pivot['overhead_percent'].plot(kind='bar', figsize=(10, 6), rot=45, color='coral')
    ax.set_ylabel('FUSE Overhead (%)')
    ax.set_xlabel('Test')
    ax.set_title('FUSE Performance Overhead vs tmpfs Baseline')
    ax.axhline(y=0, color='black', linestyle='-', linewidth=0.5)
    ax.grid(axis='y', alpha=0.3)
    
    # Add value labels on bars
    for i, v in enumerate(pivot['overhead_percent']):
        ax.text(i, v + 1, f'{v:.1f}%', ha='center', va='bottom')
    
    plt.tight_layout()
    plt.savefig('overhead_percentage.png', dpi=300)
    print("Saved: overhead_percentage.png")
    plt.close()

def plot_files_per_second(df):
    """Compare file operation rates"""
    # Filter tests with files_per_sec data
    file_tests = df[df['files_per_sec'] != 'N/A'].copy()
    file_tests['files_per_sec'] = pd.to_numeric(file_tests['files_per_sec'])
    
    # Pivot data
    pivot = file_tests.pivot(index='test', columns='filesystem', values='files_per_sec')
    
    # Create bar chart
    ax = pivot.plot(kind='bar', figsize=(12, 6), rot=45)
    ax.set_ylabel('Operations per Second')
    ax.set_xlabel('Test')
    ax.set_title('File Operations Rate: FUSE vs tmpfs')
    ax.legend(title='Filesystem')
    ax.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('files_per_second.png', dpi=300)
    print("Saved: files_per_second.png")
    plt.close()

def generate_summary_table(df):
    """Generate a summary comparison table"""
    print("\n" + "="*80)
    print("BENCHMARK SUMMARY")
    print("="*80)
    
    for test in df['test'].unique():
        print(f"\n{test.upper().replace('_', ' ')}:")
        test_data = df[df['test'] == test]
        
        for _, row in test_data.iterrows():
            fs = row['filesystem']
            if row['throughput_mbps'] != 'N/A':
                print(f"  {fs:10s}: {row['throughput_mbps']:>8.2f} MB/s, "
                      f"{row['avg_latency_ms']:>7.2f} ms latency")
            else:
                print(f"  {fs:10s}: {row['files_per_sec']:>8.2f} files/sec, "
                      f"{row['avg_latency_ms']:>7.2f} ms latency")
    
    print("\n" + "="*80)

def main():
    print("FUSE Benchmark Results Plotter")
    print("=" * 50)
    
    # Load data
    df = load_results()
    
    # Generate plots
    print("\nGenerating graphs...")
    plot_throughput_comparison(df)
    plot_latency_comparison(df)
    plot_overhead_percentage(df)
    plot_files_per_second(df)
    
    # Print summary
    generate_summary_table(df)
    
    print("\n✓ All graphs generated successfully!")
    print("\nGenerated files:")
    print("  - throughput_comparison.png")
    print("  - latency_comparison.png")
    print("  - overhead_percentage.png")
    print("  - files_per_second.png")

if __name__ == '__main__':
    main()