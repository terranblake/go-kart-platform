#!/usr/bin/env python3
""" Continuously monitors and displays aggregated stats from the telemetry DB. """

import sqlite3
import time
import os
import sys
import logging
import configparser
import argparse

# Add project root for shared imports
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

from shared.lib.python.can.protocol_registry import ProtocolRegistry

# Basic logging setup for the tool
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

def get_db_path_from_config(config_path):
    """Reads the DB_PATH from a specified collector config file."""
    try:
        parser = configparser.ConfigParser()
        # Construct absolute path if relative
        abs_config_path = os.path.abspath(config_path)
        if not os.path.exists(abs_config_path):
            logger.error(f"Config file not found at {abs_config_path}")
            return None
            
        parser.read(abs_config_path)
        db_path = parser.get('DEFAULT', 'DB_PATH', fallback=None)
        if db_path:
             # If DB path in config is relative, make it relative to config file location
             if not os.path.isabs(db_path):
                  config_dir = os.path.dirname(abs_config_path)
                  db_path = os.path.join(config_dir, db_path)
             return os.path.abspath(db_path)
        else:
            logger.error(f"DB_PATH not found in config file {abs_config_path}")
            return None
    except Exception as e:
        logger.error(f"Error reading config {config_path}: {e}")
        return None

def get_stats(db_path, protocol_registry):
    """Connects to the DB and fetches aggregated statistics and current state."""
    stats = {
        'message_types': {},
        'component_types': {},
        'component_ids': {},
        'total_records': 0,
        'current_state': {}, # Add dictionary to hold current state
        'db_size_bytes': 0, # Add field for total size
        'avg_record_size_bytes': 0.0, # Add field for average size
        'avg_time_delta_ms': 0.0, # Avg time between consecutive records (recorded_at)
        'avg_processing_latency_ms': 0.0, # Avg (received_at - recorded_at)
        'avg_db_insert_latency_ms': 0.0, # Avg (created_at - received_at)
        'latest_created_at': 0.0, # Track latest insertion time
        'latest_received_at': 0.0, # Track latest receive time
        'latest_recorded_at': 0.0, # Track latest record time
        'error': None
    }
    if not os.path.exists(db_path):
        stats['error'] = f"Database file not found: {db_path}"
        return stats

    conn = None
    try:
        conn = sqlite3.connect(f'file:{db_path}?mode=ro', uri=True, timeout=5) # Read-only mode
        conn.row_factory = sqlite3.Row
        cursor = conn.cursor()

        # Total Records
        cursor.execute("SELECT COUNT(*) FROM telemetry_history")
        stats['total_records'] = cursor.fetchone()[0]

        # Message Type Counts
        cursor.execute("SELECT message_type, COUNT(*) FROM telemetry_history GROUP BY message_type")
        for msg_type_id, count in cursor.fetchall():
            name = protocol_registry.get_message_type_name(msg_type_id)
            stats['message_types'][name] = count

        # Component Type Counts
        cursor.execute("SELECT component_type, COUNT(*) FROM telemetry_history GROUP BY component_type")
        for comp_type_id, count in cursor.fetchall():
            name = protocol_registry.get_component_type_name(comp_type_id)
            stats['component_types'][name] = count

        # Component ID Counts (Grouped by Type)
        cursor.execute("SELECT component_type, component_id, COUNT(*) FROM telemetry_history GROUP BY component_type, component_id")
        for comp_type_id, comp_id, count in cursor.fetchall():
            type_name = protocol_registry.get_component_type_name(comp_type_id)
            id_name = protocol_registry.get_component_id_name(comp_type_id, comp_id)
            full_name = f"{type_name}/{id_name}"
            stats['component_ids'][full_name] = count
        
        # Fetch current state (latest record for each component/command)
        cursor.execute("""
            SELECT 
                t.recorded_at, t.message_type, t.component_type,
                t.component_id, t.command_id, t.value_type, t.value 
            FROM telemetry_history t
            INNER JOIN (
                SELECT component_type, component_id, command_id, MAX(recorded_at) as max_ts
                FROM telemetry_history 
                GROUP BY component_type, component_id, command_id
            ) latest 
            ON t.component_type = latest.component_type 
            AND t.component_id = latest.component_id 
            AND t.command_id = latest.command_id 
            AND t.recorded_at = latest.max_ts;
        """)
        current_state_rows = cursor.fetchall()
        # Process current state into nested dict
        kart_state_tree = {}
        for row in current_state_rows:
            comp_type_name = protocol_registry.get_component_type_name(row['component_type'])
            comp_id_name = protocol_registry.get_component_id_name(row['component_type'], row['component_id'])
            command_name = protocol_registry.get_command_name(row['component_type'], row['command_id'])
            value = row['value'] # Assume raw value is okay for display for now
            
            if comp_type_name not in kart_state_tree:
                kart_state_tree[comp_type_name] = {}
            if comp_id_name not in kart_state_tree[comp_type_name]:
                kart_state_tree[comp_type_name][comp_id_name] = {}
            
            kart_state_tree[comp_type_name][comp_id_name][command_name] = value
            
        stats['current_state'] = kart_state_tree

        # Calculate average time delta between records (based on recorded_at)
        if stats['total_records'] > 1:
            cursor.execute("SELECT recorded_at FROM telemetry_history ORDER BY recorded_at ASC")
            timestamps = [row[0] for row in cursor.fetchall()]
            if len(timestamps) > 1:
                diffs = [timestamps[i] - timestamps[i-1] for i in range(1, len(timestamps))]
                avg_diff_s = sum(diffs) / len(diffs)
                stats['avg_time_delta_ms'] = avg_diff_s * 1000
        
        # Calculate average latencies in Python based on recent records
        if stats['total_records'] > 0:
            limit_for_avg = 1000 # Calculate average based on last 1000 records
            # Fetch raw timestamps for recent records
            cursor.execute(f"""
                SELECT recorded_at, received_at, created_at 
                FROM telemetry_history 
                WHERE created_at IS NOT NULL AND received_at IS NOT NULL AND recorded_at IS NOT NULL
                ORDER BY id DESC -- Get most recent records
                LIMIT {limit_for_avg}
            """)
            recent_timestamps = cursor.fetchall()
            
            processing_latencies = []
            insert_latencies = []
            latest_created = 0
            latest_received = 0
            latest_recorded = 0

            for row in recent_timestamps:
                recorded = row['recorded_at']
                received = row['received_at']
                created = row['created_at'] # Should be REAL now
                
                # Basic sanity check for realistic values
                if received >= recorded and created >= received:
                    processing_latencies.append(received - recorded)
                    insert_latencies.append(created - received)
                    
                # Track latest times
                latest_recorded = max(latest_recorded, recorded)
                latest_received = max(latest_received, received)
                latest_created = max(latest_created, created)

            if processing_latencies:
                stats['avg_processing_latency_ms'] = (sum(processing_latencies) / len(processing_latencies)) * 1000
            if insert_latencies:
                stats['avg_db_insert_latency_ms'] = (sum(insert_latencies) / len(insert_latencies)) * 1000
                
            stats['latest_created_at'] = latest_created
            stats['latest_received_at'] = latest_received
            stats['latest_recorded_at'] = latest_recorded

        # Get total file size
        try:
            stats['db_size_bytes'] = os.path.getsize(db_path)
            if stats['total_records'] > 0:
                stats['avg_record_size_bytes'] = stats['db_size_bytes'] / stats['total_records']
        except OSError as e:
            logger.warning(f"Could not get file size for {db_path}: {e}")

    except sqlite3.Error as e:
        stats['error'] = f"Database query error: {e}"
        logger.error(stats['error'])
    except Exception as e:
         stats['error'] = f"Unexpected error: {e}"
         logger.error(stats['error'])
    finally:
        if conn:
            conn.close()
    return stats

def display_stats(stats):
    """Clears the screen and displays the fetched statistics."""
    os.system('cls' if os.name == 'nt' else 'clear')
    print("--- Telemetry DB Statistics ---")
    print(f"Timestamp: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    if stats['error']:
        print(f"ERROR: {stats['error']}")
        return

    # --- Display Current State Tree --- 
    print("\n--- Current Kart State ---")
    if stats.get('current_state'):
        print("kart")
        _print_state_tree(stats['current_state'], indent=1)
    else:
        print("  (No state data available)")

    # --- Display Aggregated Stats --- 
    print("\n--- Aggregated Statistics ---")
    print(f"Total Records: {stats['total_records']:,}")
    print(f"DB Size: {stats.get('db_size_bytes', 0):,} bytes")
    print(f"Avg Record Size: {stats.get('avg_record_size_bytes', 0):.2f} bytes")
    print(f"Avg Time Between Records (recorded_at): {stats.get('avg_time_delta_ms', 0):.3f} ms")
    print(f"Avg Processing Latency (receive-record): {stats.get('avg_processing_latency_ms', 0):.3f} ms")
    print(f"Avg DB Insert Latency (create-receive): {stats.get('avg_db_insert_latency_ms', 0):.3f} ms")
    # Add latest timestamp display
    print(f"Latest Recorded At: {time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(stats.get('latest_recorded_at', 0))) if stats.get('latest_recorded_at') else 'N/A'}")
    print(f"Latest Received At: {time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(stats.get('latest_received_at', 0))) if stats.get('latest_received_at') else 'N/A'}")
    print(f"Latest Created At:  {time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(stats.get('latest_created_at', 0))) if stats.get('latest_created_at') else 'N/A'}")
    print("\nMessage Types:")
    if stats['message_types']:
        for name, count in sorted(stats['message_types'].items()):
            print(f"  - {name}: {count:,}")
    else:
        print("  (No data)")

    print("\nComponent Types:")
    if stats['component_types']:
        for name, count in sorted(stats['component_types'].items()):
            print(f"  - {name}: {count:,}")
    else:
        print("  (No data)")

    print("\nComponent IDs:")
    if stats['component_ids']:
        for name, count in sorted(stats['component_ids'].items()):
            print(f"  - {name}: {count:,}")
    else:
        print("  (No data)")
    print("\n(Press Ctrl+C to exit)")

def _print_state_tree(node, indent=0):
    """Recursively prints the state tree with indentation (compact format)."""
    prefix = "  " * indent
    for key, value in sorted(node.items()):
        if isinstance(value, dict):
            # Check if the next level contains command:value pairs or further nesting
            is_component_id_level = all(not isinstance(v, dict) for v in value.values())
            
            if is_component_id_level:
                 # Compact display: Print component ID and commands/values on one line
                 value_str = ", ".join(f"{cmd}:{val}" for cmd, val in sorted(value.items()))
                 print(f"{prefix}- {key}: {{{value_str}}}")
            else:
                 # Normal recursive display for deeper levels (e.g., component type)
                 print(f"{prefix}- {key}")
                 _print_state_tree(value, indent + 1)
        # else: # Should not happen with current structure, root call passes the dict
        #     print(f"{prefix}  - {key}: {value}") 

def main():
    parser = argparse.ArgumentParser(description="Monitor Telemetry Database Statistics")
    parser.add_argument(
        '--config', 
        type=str, 
        default='telemetry/collector/config_remote.ini', 
        help='Path to the collector config file to find the DB path (default: telemetry/collector/config_remote.ini)'
    )
    parser.add_argument(
        '--interval', 
        type=float,
        default=5.0,
        help='Update interval in seconds (default: 5)'
    )
    args = parser.parse_args()

    db_path = 'remote_telemetry_test.db'
    if not db_path:
        sys.exit(1)
        
    logger.info(f"Monitoring database: {db_path}")
    logger.info(f"Update interval: {args.interval} seconds")

    try:
        protocol = ProtocolRegistry()
    except Exception as e:
        logger.error(f"Failed to initialize ProtocolRegistry: {e}")
        sys.exit(1)

    try:
        while True:
            stats = get_stats(db_path, protocol)
            display_stats(stats)
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nExiting stats monitor.")
    except Exception as e:
        logger.error(f"Stats monitor failed: {e}", exc_info=True)

if __name__ == "__main__":
    main() 