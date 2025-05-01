""" Uplink Manager for streaming telemetry data via WebSocket """

import asyncio
import json
import logging
import threading
import time
import websockets
from websockets.exceptions import ConnectionClosed, ConnectionClosedOK, WebSocketException
from shared.lib.python.telemetry.persistent_store import PersistentTelemetryStore
# Assuming CANInterfaceWrapper is needed for reporting status
# from shared.lib.python.can.interface import CANInterfaceWrapper 

logger = logging.getLogger(__name__)

class UplinkManager(threading.Thread):
    def __init__(self, store: PersistentTelemetryStore, remote_url: str,
                 # can_interface: CANInterfaceWrapper, # Pass CAN interface for reporting
                 batch_size: int = 50,
                 reconnect_delay: int = 5,
                 status_report_interval: int = 10,
                 pruning_retention_s: int = 3600, # Passed from collector
                 pruning_interval_s: int = 300): # How often to check for pruning
        super().__init__(daemon=True, name="UplinkManager")
        self.store = store
        self.remote_url = remote_url
        # self.can_interface = can_interface
        self.batch_size = batch_size
        self.reconnect_delay = reconnect_delay
        self.status_report_interval = status_report_interval
        self.pruning_retention_s = pruning_retention_s # Needed if store doesn't hold it
        self.pruning_interval_s = pruning_interval_s

        self._stop_event = threading.Event()
        self._connection_status = "DISCONNECTED"
        self._send_queue = asyncio.Queue() # Queue for records to be sent
        self._ack_queue = asyncio.Queue() # Queue for timestamps awaiting acknowledgement
        self._pending_acks = {} # Dictionary to track sent records awaiting ack {id: send_time}
        self._latency_samples = []
        self._throughput_samples = [] # Store (bytes, time_delta)
        self._last_status_report_time = 0
        self._last_prune_time = 0

    def stop(self):
        self._stop_event.set()

    def _update_connection_status(self, status: str):
        if self._connection_status != status:
            self._connection_status = status
            logger.info(f"Uplink status changed: {status}")
            # TODO: Report status via CAN using self.can_interface
            # Need ComponentId and CommandId for SYSTEM_MONITOR/UPLINK_MANAGER
            # e.g., self.can_interface.send_message(..., UplinkStatusValue[status], ...)

    async def _handle_acks(self, websocket):
        """Task to handle incoming acknowledgements from the server."""
        logger.debug("ACK HANDLER: Starting loop...")
        if not websocket:
            logger.error("ACK HANDLER: Invalid websocket object received.")
            return

        while True: # Loop indefinitely, rely on exceptions to break
            try:
                logger.debug("ACK HANDLER: Waiting for message...")
                message = await asyncio.wait_for(websocket.recv(), timeout=1.0) # Use recv to detect closure
                logger.debug(f"ACK HANDLER: Received message: {message[:100]}...")
                ack_data = json.loads(message)
                # Expecting status: ok and processed_ids list
                if ack_data.get("status") == "ok" and "processed_ids" in ack_data:
                    received_time = time.time()
                    acked_ids = ack_data["processed_ids"]
                    valid_acked_ids = []
                    for record_id in acked_ids:
                        if record_id in self._pending_acks:
                            send_time = self._pending_acks.pop(record_id)
                            latency = (received_time - send_time) * 1000 # ms
                            self._latency_samples.append(latency)
                            # Throughput calc needs message size - estimate or track?
                            valid_acked_ids.append(record_id)
                        else:
                            logger.warning(f"Received ack for unknown record ID: {record_id}")

                    if valid_acked_ids:
                        self.store.mark_records_uploaded(valid_acked_ids) # Use IDs now
                        logger.debug(f"Successfully processed ack for {len(valid_acked_ids)} record IDs.")
                        # Update last ack ID if needed for resuming (optional for now)
                        # self._last_acked_id = max(self._last_acked_id, max(valid_acked_ids))
                else:
                    logger.warning(f"Received non-ok status or invalid ack format: {ack_data}")

            except asyncio.TimeoutError:
                # logger.debug("ACK HANDLER: Timeout waiting for message.")
                continue # No message received, continue loop
            except asyncio.CancelledError:
                 logger.debug("ACK HANDLER: Task cancelled.")
                 break
            except websockets.ConnectionClosed:
                logger.warning("ACK HANDLER: Connection closed while waiting for ack.")
                break
            except Exception as e:
                logger.error(f"ACK HANDLER: Error processing ack: {e}", exc_info=True)
                break # Exit ack handling on error
        logger.debug("ACK HANDLER: Exiting loop.")

    async def _send_records(self, websocket):
        """Task to fetch records from DB and send them via WebSocket."""
        logger.debug("SEND RECORDS: Starting loop...")
        if not websocket:
            logger.error("SEND RECORDS: Invalid websocket object received.")
            return

        try: # Wrap entire loop logic for finally
            while True: # Loop indefinitely, rely on exceptions to break
                logger.debug("SEND RECORDS: Checking for records to send...")
                # Fetch a batch of un-uploaded records
                records_to_send = self.store.get_unuploaded_records(limit=self.batch_size)

                if not records_to_send:
                    logger.debug("SEND RECORDS: No records found, sleeping...")
                    # Wait even if no records initially to keep the connection alive
                    try:
                        await asyncio.sleep(1)
                    except asyncio.CancelledError:
                        logger.debug("SEND RECORDS: Task cancelled during sleep.")
                        break # Exit if cancelled
                    continue

                try:
                    logger.debug(f"SEND RECORDS: Preparing to send {len(records_to_send)} records...")
                    message = json.dumps(records_to_send)
                    send_time = time.time()
                    await websocket.send(message)
                    # Add sent records to pending acknowledgements, using record ID as key
                    batch_ids = []
                    for record in records_to_send:
                        record_id = record['id'] # Get the record ID
                        self._pending_acks[record_id] = send_time # Use ID as key
                        batch_ids.append(record_id)
                    logger.debug(f"SEND RECORDS: Sent batch IDs {batch_ids}. Pending acks: {len(self._pending_acks)}")
                    # Limit pending acks size?
                    if len(self._pending_acks) > self.batch_size * 5: # Example limit
                        logger.warning("SEND RECORDS: High number of pending acks, may indicate network issues or slow remote processing.")
                        await asyncio.sleep(0.5) # Slow down sending if too many pending

                except asyncio.CancelledError:
                     logger.debug("SEND RECORDS: Task cancelled during send.")
                     break
                except websockets.ConnectionClosed:
                    logger.warning("SEND RECORDS: Connection closed while sending records.")
                    # Put unsent records back? DB query will pick them up again.
                    break
                except Exception as e:
                    logger.error(f"SEND RECORDS: Error sending records: {e}", exc_info=True)
                    # Records weren't acked, they'll be fetched again
                    break # Exit sending loop
        finally:
             logger.debug("SEND RECORDS: Exiting loop.")

    async def _perform_periodic_tasks(self):
         """Handles tasks like status reporting and pruning."""
         logger.debug("PERIODIC: Starting loop...")
         try: # Wrap loop for finally block
             while not self._stop_event.is_set():
                 now = time.time()

                 # --- Status Reporting --- 
                 # TODO: Implement CAN status reporting logic here
                 # if now - self._last_status_report_time > self.status_report_interval:
                 #    # Calculate stats (queue size, avg latency/throughput)
                 #    # Send CAN messages
                 #    self._last_status_report_time = now
                 #    # Reset/manage samples (_latency_samples, _throughput_samples)

                 # --- Pruning --- 
                 if now - self._last_prune_time > self.pruning_interval_s:
                     logger.info("PERIODIC: Running prune check...")
                     try:
                         self.store.prune_uploaded_records() 
                     except Exception as e:
                         logger.error(f"Error during periodic pruning: {e}", exc_info=True)
                     self._last_prune_time = now
             
                 # Sleep until next check interval
                 try:
                     await asyncio.sleep(min(self.status_report_interval, self.pruning_interval_s))
                 except asyncio.CancelledError:
                     logger.debug("PERIODIC: Task cancelled during sleep.")
                     break # Exit if cancelled
         finally:
             logger.debug("PERIODIC: Exiting loop.")

    async def _manage_connection(self):
        """Maintains the WebSocket connection and runs send/receive/periodic tasks."""
        logger.debug("MANAGE CONN: Starting periodic tasks...")
        periodic_task = asyncio.create_task(self._perform_periodic_tasks())

        while not self._stop_event.is_set():
            logger.debug("MANAGE CONN: Attempting connection...")
            self._update_connection_status("CONNECTING")
            try:
                # Simplify connection: Use standard async with connect, add open_timeout
                async with websockets.connect(self.remote_url, open_timeout=10) as websocket:
                    # The 'websocket' object here should be the WebSocketClientProtocol
                    # --- Remove explicit type check --- 
                    # logger.debug(f"MANAGE CONN: Websocket object obtained. Type: {type(websocket)}")
                    # is_correct_type = isinstance(websocket, websockets.WebSocketClientProtocol)
                    # logger.debug(f"MANAGE CONN: Is expected type (WebSocketClientProtocol)? {is_correct_type}")
                    # if not is_correct_type:
                    #     logger.error("MANAGE CONN: Websocket object is NOT the expected type!")
                    #     # Skip further processing if type is wrong
                    #     continue 
                    
                    logger.debug("MANAGE CONN: Connection established.")
                    # Add a tiny delay to allow connection state to settle?
                    await asyncio.sleep(0.01)
                    self._update_connection_status("CONNECTED")
                    self._pending_acks.clear()
                    logger.debug("MANAGE CONN: Starting gather for send/receive tasks...")
                    try:
                        # --- Restore gather call --- 
                        logger.debug("MANAGE CONN: Running gather for _handle_acks and _send_records...")
                        await asyncio.gather(
                            self._handle_acks(websocket),
                            self._send_records(websocket)
                        )
                        logger.warning("MANAGE CONN: Gather finished unexpectedly (tasks might have exited normally?).")
                    except Exception as task_exc: # Renamed variable for clarity
                        # Catch exceptions raised from the task itself
                        logger.error(f"MANAGE CONN: Error during task execution: {task_exc}", exc_info=True)
                    # If the task finishes, the async with block will exit
                    # and automatically handle closing the websocket.

            except asyncio.TimeoutError:
                 logger.warning("MANAGE CONN: Connection attempt timed out.")
                 self._update_connection_status("ERROR")
            except (websockets.exceptions.WebSocketException, OSError, ConnectionRefusedError) as e:
                logger.warning(f"MANAGE CONN: WebSocket connection failed: {e}")
                self._update_connection_status("ERROR")
            except Exception as e:
                # Catch potential errors during connect or within the context manager setup
                logger.error(f"MANAGE CONN: Unexpected connection/management error: {e}", exc_info=True)
                self._update_connection_status("ERROR")
            finally:
                # No need to manually close websocket here, async with handles it.
                # Just update status and wait before retry
                if self._connection_status != "ERROR":
                     self._update_connection_status("DISCONNECTED")
                if not self._stop_event.is_set():
                    logger.info(f"MANAGE CONN: Attempting reconnect in {self.reconnect_delay} seconds...")
                    await asyncio.sleep(self.reconnect_delay) # Use asyncio.sleep

        # Cleanup outside the loop
        logger.info("MANAGE CONN: Stopping periodic tasks...")
        periodic_task.cancel()
        try:
            await periodic_task
        except asyncio.CancelledError:
            logger.info("MANAGE CONN: Periodic task successfully cancelled.")
        logger.info("UplinkManager stopped.")

    def run(self):
        """Runs the asyncio event loop for the WebSocket connection."""
        logger.info("Starting UplinkManager thread.")
        try:
            asyncio.run(self._manage_connection())
        except Exception as e:
            logger.error(f"UplinkManager run error: {e}", exc_info=True) 