import argparse
import os
import re
import matplotlib.pyplot as plt

def parse_concurrency_sweep(file_path):
    results = []
    with open(file_path, 'r') as f:
        content = f.read()
    
    # Very basic parsing based on output format
    csv_lines = re.findall(r'CSV: (.*)', content)
    for line in csv_lines:
        parts = line.split(',')
        if len(parts) == 5:
            results.append({
                'threads': int(parts[0]),
                'throughput': float(parts[1]),
                'p50': float(parts[2]),
                'p95': float(parts[3]),
                'p99': float(parts[4])
            })
    return results

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dir', required=True, help='Results directory')
    args = parser.parse_args()

    # Find the concurrency sweep file
    sweep_files = [f for f in os.listdir(args.dir) if f.startswith('concurrency_sweep_')]
    if not sweep_files:
        print("No sweep files found.")
        return

    sweep_file = os.path.join(args.dir, sweep_files[0])
    results = parse_concurrency_sweep(sweep_file)
    
    if not results:
        print("No data parsed.")
        return

    threads = [r['threads'] for r in results]
    throughput = [r['throughput'] for r in results]
    p50 = [r['p50'] for r in results]
    p95 = [r['p95'] for r in results]
    
    plt.figure(figsize=(10, 6))
    plt.bar([str(t) for t in threads], throughput)
    plt.title('Throughput vs Concurrency')
    plt.xlabel('Threads')
    plt.ylabel('Throughput (ops/sec)')
    plt.savefig(os.path.join(args.dir, 'throughput_vs_concurrency.png'))
    
    plt.figure(figsize=(10, 6))
    x = range(len(threads))
    plt.bar([i - 0.2 for i in x], p50, width=0.4, label='p50')
    plt.bar([i + 0.2 for i in x], p95, width=0.4, label='p95')
    plt.xticks(x, [str(t) for t in threads])
    plt.title('Latency vs Concurrency')
    plt.xlabel('Threads')
    plt.ylabel('Latency (us)')
    plt.legend()
    plt.savefig(os.path.join(args.dir, 'latency_vs_concurrency.png'))

if __name__ == '__main__':
    main()
