#!/usr/bin/env python3
"""
Load Balancer Log Visualization Script

Parses logs/loadbalancer.log (VERBOSE mode) and creates visualization of
dynamic server scaling over time.
"""

import re
import sys
from collections import defaultdict
import matplotlib.pyplot as plt


def parse_log_file(filename):
    """
    Parse the log file and extract server count data.
    
    Returns:
        dict: Data structure with server_history per LB
    """
    # Regex patterns
    scaling_pattern = re.compile(r'\[LB:(\d+)\]\[Cycle:(\d+)\] Scaled (?:UP|DOWN) to (\d+) servers')
    init_pattern = re.compile(r'\[LB:(\d+)\]\[Cycle:0\] Initialized with (\d+) servers')
    
    # Data structure: {lb_id: {'server_history': {cycle: count}, 'initial_servers': N}}
    data = defaultdict(lambda: {
        'server_history': {},
        'initial_servers': 0
    })
    
    try:
        with open(filename, 'r') as f:
            for line in f:
                # Check for initialization
                match = init_pattern.search(line)
                if match:
                    lb_id = int(match.group(1))
                    initial_servers = int(match.group(2))
                    data[lb_id]['initial_servers'] = initial_servers
                    data[lb_id]['server_history'][0] = initial_servers
                    continue
                
                # Check for scaling events
                match = scaling_pattern.search(line)
                if match:
                    lb_id = int(match.group(1))
                    cycle = int(match.group(2))
                    server_count = int(match.group(3))
                    data[lb_id]['server_history'][cycle] = server_count
                    continue
    
    except FileNotFoundError:
        print(f"Error: Log file '{filename}' not found.")
        print("Make sure to run the simulation with log_level=0 (VERBOSE) first.")
        sys.exit(1)
    
    return dict(data)


def fill_server_history(server_changes, max_cycle, initial_count):
    """
    Forward-fill server count for all cycles from 0 to max_cycle.
    
    Args:
        server_changes: dict of {cycle: server_count} for cycles where changes occurred
        max_cycle: maximum cycle number to fill up to
        initial_count: initial server count at cycle 0
    
    Returns:
        tuple: (cycles list, server_counts list)
    """
    cycles = []
    server_counts = []
    
    current_count = initial_count
    
    for cycle in range(max_cycle + 1):
        if cycle in server_changes:
            current_count = server_changes[cycle]
        cycles.append(cycle)
        server_counts.append(current_count)
    
    return cycles, server_counts


def plot_data(data, lb_id):
    """
    Create visualization of server count over time.
    
    Args:
        data: Data dictionary for a specific LoadBalancer
        lb_id: LoadBalancer ID for title
    """
    server_history = data['server_history']
    initial_servers = data['initial_servers']
    
    if not server_history:
        print(f"No data found for LoadBalancer {lb_id}")
        return
    
    # Determine max cycle from available data
    max_cycle = max(server_history.keys())
    
    # Prepare server count data (forward-fill)
    server_cycles, server_counts = fill_server_history(server_history, max_cycle, initial_servers)
    
    # Create figure with single plot
    fig, ax = plt.subplots(1, 1, figsize=(14, 6))
    fig.suptitle(f'Load Balancer {lb_id} - Dynamic Server Scaling', fontsize=16, fontweight='bold')
    
    # Plot server count over time
    ax.plot(server_cycles, server_counts, color='forestgreen', linewidth=2, drawstyle='steps-post', label='Active Servers')
    ax.fill_between(server_cycles, server_counts, alpha=0.3, color='forestgreen', step='post')
    ax.set_xlabel('Cycle', fontsize=12)
    ax.set_ylabel('Server Count', fontsize=12)
    ax.grid(True, alpha=0.3)
    ax.legend(loc='upper right')
    
    # Add some statistics as text
    avg_servers = sum(server_counts) / len(server_counts)
    max_servers = max(server_counts)
    min_servers = min(server_counts)
    
    stats_text = f'Avg Servers: {avg_servers:.1f}\nRange: {min_servers}-{max_servers}'
    fig.text(0.02, 0.02, stats_text, fontsize=10, verticalalignment='bottom',
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    
    # Save the figure
    output_file = f'loadbalancer_{lb_id}_visualization.png'
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"Visualization saved to: {output_file}")
    
    # Display the interactive window
    plt.show()


def main():
    """Main entry point for the visualization script."""
    log_file = 'logs/loadbalancer.log'
    
    if len(sys.argv) > 1:
        log_file = sys.argv[1]
    
    print(f"Parsing log file: {log_file}")
    data = parse_log_file(log_file)
    
    if not data:
        print("No data found in log file. Make sure the simulation ran with log_level=0 (VERBOSE).")
        return
    
    print(f"Found data for {len(data)} LoadBalancer(s)")
    
    # Plot data for each LoadBalancer
    for lb_id in sorted(data.keys()):
        print(f"\nGenerating visualization for LoadBalancer {lb_id}...")
        plot_data(data[lb_id], lb_id)


if __name__ == '__main__':
    main()
