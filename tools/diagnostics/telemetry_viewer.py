#!/usr/bin/env python3
"""
Telemetry Viewer - Simple tool to view telemetry data from the collector API
"""

import sys
import argparse
import time
import json
import logging
import requests
from datetime import datetime
from prettytable import PrettyTable

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class TelemetryViewer:
    """Simple viewer for telemetry data from the collector API"""
    
    def __init__(self, api_url, update_interval=1.0):
        """
        Initialize the telemetry viewer
        
        Args:
            api_url: Base URL for the telemetry collector API
            update_interval: Interval between updates in seconds
        """
        self.api_url = api_url.rstrip('/')
        self.update_interval = update_interval
        self.running = False
        
    def fetch_current_state(self):
        """Fetch the current state from the API"""
        try:
            response = requests.get(f"{self.api_url}/api/state/current", timeout=5)
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            logger.error(f"Error fetching current state: {e}")
            return None
    
    def fetch_history(self, limit=10):
        """Fetch history data from the API"""
        try:
            response = requests.get(f"{self.api_url}/api/state/history?limit={limit}", timeout=5)
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            logger.error(f"Error fetching history: {e}")
            return []
    
    def fetch_component_state(self, comp_type, comp_id):
        """Fetch state for a specific component"""
        try:
            url = f"{self.api_url}/api/state/component/{comp_type}/{comp_id}"
            response = requests.get(url, timeout=5)
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            logger.error(f"Error fetching component state: {e}")
            return None
    
    def display_current_state(self, state):
        """Display the current state in a table format"""
        if not state:
            print("No current state available")
            return
            
        # Convert timestamp to human-readable format
        if 'timestamp' in state:
            timestamp = datetime.fromtimestamp(state['timestamp']).strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
        else:
            timestamp = "Unknown"
            
        print(f"\n=== Current State ({timestamp}) ===")
        
        table = PrettyTable()
        table.field_names = ["Key", "Value"]
        
        # Add rows sorted by key
        for key in sorted(state.keys()):
            value = state[key]
            # Truncate long values
            if isinstance(value, str) and len(value) > 30:
                value = value[:27] + "..."
            table.add_row([key, value])
            
        print(table)
    
    def display_history(self, history, limit=10):
        """Display the telemetry history in a table format"""
        if not history:
            print("No history available")
            return
            
        # Limit to the most recent entries
        history = history[:limit]
        
        print(f"\n=== Last {len(history)} Telemetry Updates ===")
        
        # Create table with dynamic columns based on available keys
        table = PrettyTable()
        
        # Get all unique keys
        keys = set()
        for entry in history:
            keys.update(entry.keys())
            
        # Prioritize certain keys to show first
        priority_keys = ['timestamp', 'message_type', 'component_type', 'component_id', 'command_id', 'value']
        ordered_keys = []
        
        # Add priority keys first if they exist
        for key in priority_keys:
            if key in keys:
                ordered_keys.append(key)
                keys.remove(key)
                
        # Add remaining keys
        ordered_keys.extend(sorted(keys))
        
        # Set field names
        table.field_names = ordered_keys
        
        # Add rows
        for entry in history:
            row = []
            for key in ordered_keys:
                value = entry.get(key, "")
                # Format timestamp
                if key == 'timestamp' and value:
                    value = datetime.fromtimestamp(value).strftime('%H:%M:%S')
                # Truncate long values
                if isinstance(value, str) and len(value) > 15:
                    value = value[:12] + "..."
                row.append(value)
            table.add_row(row)
            
        print(table)
    
    def start_live_view(self, view_type='current', component_type=None, component_id=None, refresh_interval=1.0):
        """
        Start a live updating view of telemetry data
        
        Args:
            view_type: Type of view ('current', 'history', 'component')
            component_type: Component type for component view
            component_id: Component ID for component view
            refresh_interval: Refresh interval in seconds
        """
        self.running = True
        
        try:
            while self.running:
                # Clear screen
                print("\033c", end="")
                
                # Fetch and display based on view type
                if view_type == 'current':
                    state = self.fetch_current_state()
                    self.display_current_state(state)
                elif view_type == 'history':
                    history = self.fetch_history(limit=20)
                    self.display_history(history)
                elif view_type == 'component' and component_type and component_id:
                    state = self.fetch_component_state(component_type, component_id)
                    self.display_current_state(state)
                else:
                    print(f"Unknown view type: {view_type}")
                    break
                    
                # Show instructions
                print("\nPress Ctrl+C to exit")
                
                # Wait for the next update
                time.sleep(refresh_interval)
                
        except KeyboardInterrupt:
            logger.info("Live view stopped by user")
        finally:
            self.running = False

def main():
    parser = argparse.ArgumentParser(description='Telemetry Viewer for Go-Kart Platform')
    parser.add_argument('--api-url', default='http://localhost:5001', 
                        help='Telemetry collector API URL')
    parser.add_argument('--view', choices=['current', 'history', 'component'], 
                        default='current', help='Type of view to display')
    parser.add_argument('--component-type', help='Component type for component view')
    parser.add_argument('--component-id', help='Component ID for component view')
    parser.add_argument('--refresh', type=float, default=1.0, 
                        help='Refresh interval in seconds')
    parser.add_argument('--debug', action='store_true', help='Enable debug logging')
    
    args = parser.parse_args()
    
    # Set log level
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)
    
    # Validate arguments
    if args.view == 'component' and (not args.component_type or not args.component_id):
        parser.error("--component-type and --component-id are required for component view")
    
    # Create and start the viewer
    viewer = TelemetryViewer(api_url=args.api_url, update_interval=args.refresh)
    
    logger.info(f"Starting telemetry viewer with API URL: {args.api_url}")
    viewer.start_live_view(
        view_type=args.view,
        component_type=args.component_type,
        component_id=args.component_id,
        refresh_interval=args.refresh
    )

if __name__ == '__main__':
    main()
