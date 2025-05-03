from collections import OrderedDict
import math
from typing import Optional
from shared.lib.python.telemetry.state import GoKartState
from shared.lib.python.can.interface import CANInterfaceWrapper
import threading
import logging
import time

from shared.lib.python.telemetry.store import TelemetryStore
from shared.lib.python.can.protocol_registry import ProtocolRegistry

class PingBroadcaster(threading.Thread):
    """Periodically sends PING commands to synchronize devices and enable RTT calculation."""
    def __init__(self, can_interface: CANInterfaceWrapper, telemetry_store: TelemetryStore, protocol_registry: ProtocolRegistry, interval_s: float = 1.0, stop_event: threading.Event = None):
        super().__init__(daemon=True) # Make thread daemon so it exits with main
        self.can_interface = can_interface
        self.telemetry_store = telemetry_store
        self.protocol_registry = protocol_registry
        self.interval_s = interval_s
        self.stop_event = stop_event or threading.Event()
        self.logger = logging.getLogger(__name__)
        self.logger.info(f"PingBroadcaster initialized with interval: {self.interval_s}s")

        # --- RTT Tracking ---
        # Stores { ping_value_24bit: send_timestamp_float }
        self._pending_pings = OrderedDict()
        # Stores { node_id: average_rtt_ms_float }
        self._rtt_estimates = {}
        self._max_ping_age_s = 5.0 # Prune pings older than 5 seconds
        self._rtt_alpha = 0.1 # EMA alpha for RTT smoothing
        self._ping_prune_interval = 1.0 # How often to check for old pings
        self._last_ping_prune_time = 0
        # --------------------

        # Register PONG handler
        pong_comp_type = self.protocol_registry.get_component_type('SYSTEM_MONITOR')
        pong_cmd_id = self.protocol_registry.get_command_id('SYSTEM_MONITOR', 'PONG')
        self.can_interface.register_handler('STATUS', pong_comp_type, 255, pong_cmd_id, self._handle_pong)

    def run(self):
        """Main loop to send PING commands periodically."""
        self.logger.info("PingBroadcaster thread started.")
        while not self.stop_event.is_set():
            try:
                # Prepare PING message details
                msg_type_name = 'COMMAND'
                comp_type_name = 'SYSTEM_MONITOR'
                comp_name = 'TIME_MASTER' # Conceptual sender ID for PING
                cmd_name = 'PING'
                value_type_name = 'UINT24' # Value is 24-bit ms timestamp

                # Calculate the 24-bit timestamp value
                current_time_ms = int(time.time() * 1000)
                time_value_24bit = current_time_ms & 0xFFFFFF

                ping_send_time = time.time()
                self.store_ping_send_time(time_value_24bit, ping_send_time)

                # Send the PING command (no delay override needed)
                success = self.can_interface.send_command(
                    message_type_name=msg_type_name,
                    component_type_name=comp_type_name,
                    component_name=comp_name,
                    command_name=cmd_name,
                    value_type=value_type_name,
                    direct_value=time_value_24bit,
                    delay_override=None # Set to None (or -1 handled by C++ default)
                    # destination_node_id=None # PING is broadcast
                )

                if success:
                    self.logger.debug(f"Sent PING command with value: {time_value_24bit}")
                else:
                    self.logger.warning("Failed to send PING command.")

            except Exception as e:
                self.logger.error(f"Error in PingBroadcaster loop: {e}", exc_info=True)

            self.stop_event.wait(self.interval_s)
        self.logger.info("PingBroadcaster thread stopped.")

    def stop(self):
        """Signals the thread to stop."""
        self.logger.info("Stopping PingBroadcaster thread...")
        self.stop_event.set()
    
    def _handle_pong(self, source_node_id, msg_type, comp_type, comp_id, cmd_id, val_type, value, timestamp_delta):
        """Handles incoming PONG messages to calculate RTT and send SET_TIME.
           value = original PING value (24-bit ms timestamp)
        """
        pong_received_time = time.time()
        original_ping_value = value # This is the 24-bit timestamp sent in PING

        if original_ping_value not in self._pending_pings:
            self.logger.warning(f"Received PONG from {source_node_id:#04x} with unknown PING value: {original_ping_value}. Skipping RTT update.")
            return

        ping_send_time = self._pending_pings.pop(original_ping_value)
        rtt_ms = (pong_received_time - ping_send_time) * 1000

        if rtt_ms < 0:
            self.logger.warning(f"Negative RTT calculated for node {source_node_id:#04x} (RTT: {rtt_ms:.2f} ms). Skipping RTT update.")

        # Store RTT estimate for this specific node
        if source_node_id not in self._rtt_estimates:
            self._rtt_estimates[source_node_id] = []
        self._rtt_estimates[source_node_id].append(rtt_ms)
        # Keep only the last N samples (e.g., 10)
        max_samples = 10
        self._rtt_estimates[source_node_id] = self._rtt_estimates[source_node_id][-max_samples:]
        avg_rtt = sum(self._rtt_estimates[source_node_id]) / len(self._rtt_estimates[source_node_id])
        
        self.logger.debug(f"Received PONG from {source_node_id:#04x}. RTT: {rtt_ms:.2f} ms. Avg RTT: {avg_rtt:.2f} ms.")
        
        # --- Send SET_TIME Command --- 
        # Estimate one-way delay (half RTT)
        one_way_delay_ms = rtt_ms / 2.0
        
        # Calculate the target device time: Time PONG received + estimated one-way delay
        # We need this in milliseconds since epoch
        target_device_time_ms = int((pong_received_time * 1000) + one_way_delay_ms)
        
        # Get the 24-bit value for the SET_TIME command
        target_time_ms_24bit = target_device_time_ms & 0xFFFFFF
        
        # Send the SET_TIME command specifically to the source node
        self.can_interface.send_command(
            message_type_name='COMMAND',
            component_type_name='SYSTEM_MONITOR',
            component_name='TIME_MASTER', # Logical sender ID?
            command_name='SET_TIME',
            direct_value=target_time_ms_24bit,
            delay_override=None, # No delay override needed for SET_TIME
            destination_node_id=source_node_id # Target the specific node
        )
        self.logger.debug(f"Sent SET_TIME command to {source_node_id:#04x} with value {target_time_ms_24bit} (Target epoch ms: {target_device_time_ms})")


    def store_ping_send_time(self, ping_value_24bit: int, send_timestamp: float):
        """Stores the send time for a specific PING value.
           Also triggers pruning of old pings.
        """
        now = time.time()
        self._pending_pings[ping_value_24bit] = send_timestamp
        # Prune occasionally when adding new pings
        if now - self._last_ping_prune_time > self._ping_prune_interval:
             self._prune_pending_pings(now)
             self._last_ping_prune_time = now
             
    def _prune_pending_pings(self, current_time: float):
        """Removes entries from _pending_pings older than _max_ping_age_s."""
        cutoff_time = current_time - self._max_ping_age_s
        keys_to_remove = []
        for key, send_time in self._pending_pings.items():
            if send_time < cutoff_time:
                keys_to_remove.append(key)
            else:
                # Since OrderedDict is ordered by insertion, we can stop early
                break
        if keys_to_remove:
             removed_count = 0
             for key in keys_to_remove:
                 if key in self._pending_pings: # Check if key still exists
                      del self._pending_pings[key]
                      removed_count += 1
             self.logger.debug(f"Pruned {removed_count} old PING entries.")

    def get_rtt_estimate_ms(self, node_id: int) -> Optional[int]:
        """Gets the current estimated RTT in milliseconds for a given node ID."""
        avg_rtt = self._rtt_estimates.get(node_id)
        return int(avg_rtt) if avg_rtt is not None and not math.isnan(avg_rtt) else None