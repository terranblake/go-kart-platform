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

                # --- Calculate Estimated One-Way Delay --- 
                estimated_delay_ms = 0 # Default to 0
                all_rtts = [rtt for rtt in self._rtt_estimates.values() if rtt is not None and not math.isnan(rtt)]
                if all_rtts:
                    avg_rtt = sum(all_rtts) / len(all_rtts)
                    # One-way delay is roughly RTT / 2
                    estimated_delay_ms = int(avg_rtt / 2)
                    # Clamp to 8 bits (0-255)
                    estimated_delay_ms = max(0, min(estimated_delay_ms, 255))
                    self.logger.debug(f"Calculated estimated one-way delay: {estimated_delay_ms} ms (avg RTT: {avg_rtt:.1f} ms)")
                else:
                    self.logger.debug("No valid RTT estimates available yet to calculate delay.")

                ping_send_time = time.time()
                self.store_ping_send_time(time_value_24bit, ping_send_time)
                # -------------------------------------

                # Send the PING command with the estimated delay
                success = self.can_interface.send_command(
                    message_type_name=msg_type_name,
                    component_type_name=comp_type_name,
                    component_name=comp_name,
                    command_name=cmd_name,
                    value_name=None,
                    direct_value=time_value_24bit,
                    delay_override=estimated_delay_ms # Pass calculated delay
                )

                if success:
                    self.logger.debug(f"Sent PING command with value: {time_value_24bit}, delay: {estimated_delay_ms}")
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
        """Handles incoming PONG messages to calculate RTT."""
        pong_receive_time = time.time()
        ping_value_24bit = value # Value is the echoed ping_timestamp_value

        # Look up the original send time
        send_timestamp = self._pending_pings.pop(ping_value_24bit, None)

        if send_timestamp is None:
            self.logger.warning(f"PONG received from {source_node_id:#04x} (echoed {ping_value_24bit}). No send timestamp found.")
            return

        rtt_s = pong_receive_time - send_timestamp
        # Clamp RTT to avoid negative values/extremes
        rtt_ms = max(0, min(int(rtt_s * 1000), 5000)) # Max 5 seconds RTT

        self.logger.debug(f"PONG received from {source_node_id:#04x} (echoed {ping_value_24bit}). RTT: {rtt_ms} ms.")

        # Update rolling average RTT estimate
        old_avg = self._rtt_estimates.get(source_node_id, float(rtt_ms)) # Use current as first estimate
        if math.isnan(old_avg): old_avg = float(rtt_ms)
        new_avg = self._rtt_alpha * rtt_ms + (1 - self._rtt_alpha) * old_avg
        self._rtt_estimates[source_node_id] = new_avg
        self.logger.debug(f"Updated RTT estimate for {source_node_id:#04x}: {new_avg:.2f} ms")

        if not self.telemetry_store:
            self.logger.warning("Telemetry store not available, cannot store RTT.")
            return

        # Store the RTT sample in telemetry
        try:
            rtt_state = GoKartState(
                timestamp=pong_receive_time, # Log when RTT was calculated
                message_type=self.protocol_registry.get_message_type('STATUS'),
                component_type=self.protocol_registry.get_component_type('SYSTEM_MONITOR'),
                component_id=source_node_id, # ID of the node that sent the PONG
                command_id=self.protocol_registry.get_command_id('SYSTEM_MONITOR', 'ROUNDTRIPTIME_MS'),
                value_type=self.protocol_registry.get_value_type('UINT16'), # RTT in ms
                value=rtt_ms
            )
            self.telemetry_store.update_state(rtt_state)
        except KeyError as e:
            self.logger.error(f"Failed to store RTT: Protocol key error - {e}")
        except Exception as e:
            self.logger.error(f"Failed to store RTT for node {source_node_id:#04x}: {e}", exc_info=True)

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