""" Telemetry Storage Logic (including persistence) """

import sqlite3
import logging
import time
import threading
from shared.lib.python.telemetry.store import TelemetryStore, state_to_readable_dict
from shared.lib.python.telemetry.state import GoKartState
from shared.lib.python.can.protocol_registry import ProtocolRegistry
from typing import List # Added for type hinting

logger = logging.getLogger(__name__)

class PersistentTelemetryStore(TelemetryStore):
    def __init__(self, protocol: ProtocolRegistry,
                 db_path: str = "telemetry.db",
                 max_history_records: int = 10000, # Safety cap
                 role: str = 'vehicle', # 'vehicle' or 'remote'
                 dashboard_retention_s: int = 600, # Default 10 minutes
                 local_uplinked_retention_s: int = 3600): # Added
        # Pass protocol and memory history limit to base class
        # The base class memory history is still used for immediate state access
        super().__init__(protocol, limit=dashboard_retention_s * 100) # Estimate 100 msg/sec for memory cache
        self.db_path = db_path
        self.max_history_records = max_history_records
        self.role = role
        self.dashboard_retention_s = dashboard_retention_s
        self.local_uplinked_retention_s = local_uplinked_retention_s # Store this
        self._db_lock = threading.Lock() # Use lock for thread safety
        self._duplicate_skip_count = 0 # Initialize counter for skipped duplicates
        self._init_db()
        # TODO: Load recent history from DB on startup?
        # TODO: Start background pruning thread?

    def _get_db_conn(self):
        conn = sqlite3.connect(self.db_path, timeout=10)
        conn.row_factory = sqlite3.Row # Access columns by name
        return conn

    def _init_db(self):
        logger.info(f"Initializing database at {self.db_path}")
        with self._db_lock:
            conn = self._get_db_conn()
            try:
                cursor = conn.cursor()
                # Create table if not exists with the new schema
                # Note: This won't migrate existing data if the table already exists with the old schema.
                # Manual migration or deleting the old DB file might be needed for testing.
                cursor.execute("""
                    CREATE TABLE IF NOT EXISTS telemetry_history (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        recorded_at REAL NOT NULL,       -- Timestamp from originating device/event
                        received_at REAL NOT NULL,       -- Timestamp when collector received/processed it
                        created_at REAL NOT NULL, -- Timestamp of DB insert (Unix epoch)
                        message_type INTEGER,         -- Original message fields
                        component_type INTEGER,
                        component_id INTEGER,
                        command_id INTEGER,
                        value_type INTEGER,
                        value INTEGER,
                        uplinked INTEGER DEFAULT 0 NOT NULL -- Flag for uplink status
                    )
                """)
                # Add indices for common query patterns
                cursor.execute("CREATE INDEX IF NOT EXISTS idx_recorded_at ON telemetry_history (recorded_at)")
                cursor.execute("CREATE INDEX IF NOT EXISTS idx_uplinked_created_at ON telemetry_history (uplinked, created_at)")
                
                # Attempt to drop the old primary key constraint if it exists (best effort)
                # This is tricky and might fail depending on SQLite version and previous state
                try:
                     # Check if timestamp is still PK (indicating old schema)
                     cursor.execute("PRAGMA table_info(telemetry_history)")
                     cols = cursor.fetchall()
                     is_old_schema = any(col['name'] == 'timestamp' and col['pk'] == 1 for col in cols)
                     has_id_col = any(col['name'] == 'id' for col in cols)
                     
                     if is_old_schema and not has_id_col:
                         logger.warning("Detected old schema (timestamp as PK). Data migration NOT automatically handled.")
                         # Ideally, handle migration here or instruct user to delete DB.
                         # Raising an error might be safer:
                         # raise Exception("Database schema mismatch. Please delete old telemetry.db file.")
                     # If 'id' column exists but 'timestamp' is still PK, it's even weirder.
                     # For now, we assume CREATE TABLE IF NOT EXISTS handles the basic case.
                         
                except sqlite3.Error as e:
                     logger.warning(f"Could not check/modify old schema constraints: {e}")

                # Add uplinked column if it doesn't exist (might be needed if upgrading from very old schema)
                try:
                    cursor.execute("ALTER TABLE telemetry_history ADD COLUMN uplinked INTEGER DEFAULT 0 NOT NULL")
                    logger.info("Added 'uplinked' column to telemetry_history table.")
                except sqlite3.OperationalError as e:
                    if "duplicate column name" not in str(e).lower():
                        raise e

                conn.commit()
                logger.info("Database initialized successfully with new schema.")
            except sqlite3.Error as e:
                logger.error(f"Database initialization error: {e}")
            finally:
                conn.close()

    def update_state(self, state: GoKartState):
        """Update the current state (in memory) and add to DB history"""
        # Update in-memory state (from base class)
        super().update_state(state) 
        state_dict = state.to_dict() # Use the dict from base class
        
        # --- Persist to DB ---
        with self._db_lock: 
            received_time = time.time() # Capture receive time
            logger.debug(f"STORE UPDATE: recorded_at={state.timestamp}, received_at={received_time}, diff={(received_time - state.timestamp):.4f}s") # Log timestamps
            conn = self._get_db_conn()
            try:
                cursor = conn.cursor()
                # Use INSERT OR IGNORE (though less critical now with new PK)
                cursor.execute("""
                    INSERT OR IGNORE INTO telemetry_history 
                        (recorded_at, received_at, created_at, message_type, component_type,
                         component_id, command_id, value_type, value)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                """, (
                    state.timestamp, # Original event time is recorded_at
                    received_time,   # Current time is received_at
                    time.time(),     # Use a *new* timestamp for created_at
                    state.message_type,
                    state.component_type,
                    state.component_id,
                    state.command_id,
                    state.value_type,
                    state.value
                    # uplinked defaults to 0
                ))
                # Check if the row was actually inserted or ignored
                if cursor.rowcount == 0:
                    self._duplicate_skip_count += 1
                    # Optionally log less frequently to avoid spam
                    if self._duplicate_skip_count % 100 == 1: # Log every 100 skips
                         logger.warning(f"Ignored insert potentially due to UNIQUE constraint conflict (Total skips: {self._duplicate_skip_count})")
                
                conn.commit()

                # Prune based on safety cap (MAX_HISTORY_RECORDS), now using ID
                cursor.execute("SELECT COUNT(*) FROM telemetry_history")
                count = cursor.fetchone()[0]
                if count > self.max_history_records:
                    records_to_delete = count - self.max_history_records
                    logger.warning(f"Max record limit ({self.max_history_records}) exceeded. Deleting {records_to_delete} oldest records by ID.")
                    cursor.execute(f"""
                        DELETE FROM telemetry_history
                        WHERE id IN (
                            SELECT id
                            FROM telemetry_history
                            ORDER BY id ASC -- Delete oldest by insertion order (ID)
                            LIMIT ?
                        )
                    """, (records_to_delete,))
                    conn.commit()

            except sqlite3.Error as e:
                logger.error(f"Database update error: {e}")
            finally:
                conn.close()
                
        # Update in-memory store (base class) *after* successful DB insert
        super().update_state(state)
        return state_dict # Return dict consistent with base class

    def get_unuploaded_records(self, limit: int) -> List[dict]:
        """Fetch a batch of un-uploaded records, ordered by timestamp.

        Args:
            limit: The maximum number of records to fetch.

        Returns:
            A list of telemetry records (as dictionaries including their 'id') 
            that haven't been uploaded.
        """
        with self._db_lock:
            conn = self._get_db_conn()
            try:
                cursor = conn.cursor()
                cursor.execute("""
                    SELECT id, recorded_at, message_type, component_type,
                           component_id, command_id, value_type, value 
                    FROM telemetry_history
                    WHERE uplinked = 0
                    ORDER BY id ASC -- Send oldest un-uploaded first
                    LIMIT ?
                """, (limit,))
                rows = cursor.fetchall()
                # Convert rows to dictionaries, renaming recorded_at to timestamp for GoKartState
                records = []
                for row in rows:
                    record_dict = dict(row)
                    record_dict['timestamp'] = record_dict.pop('recorded_at')
                    records.append(record_dict)
                return records
            except sqlite3.Error as e:
                logger.error(f"Database get_unuploaded_records query error: {e}")
                return []
            finally:
                conn.close()

    def mark_records_uploaded(self, ids: List[int]):
        """Mark records as uploaded in the database based on their IDs.

        Args:
            ids: A list of integer IDs corresponding to the records
                 that have been successfully uploaded.
        """
        if not ids:
            return

        with self._db_lock:
            conn = self._get_db_conn()
            try:
                cursor = conn.cursor()
                # Create placeholders for the IN clause
                placeholders = ', '.join('?' * len(ids))
                sql = f"UPDATE telemetry_history SET uplinked = 1 WHERE id IN ({placeholders})"
                cursor.execute(sql, ids)
                conn.commit()
                logger.debug(f"Marked {cursor.rowcount} records as uploaded by ID.")
            except sqlite3.Error as e:
                logger.error(f"Database mark_records_uploaded error: {e}")
            finally:
                conn.close()

    def prune_uploaded_records(self):
        """Deletes records marked as uploaded and older than the configured retention period."""
        # This uses local_uplinked_retention_s which needs to be passed in __init__
        if not hasattr(self, 'local_uplinked_retention_s') or self.local_uplinked_retention_s <= 0:
             logger.debug("Pruning of uploaded records is disabled (retention <= 0).")
             return

        cutoff_timestamp = time.time() - self.local_uplinked_retention_s
        logger.debug(f"Pruning uploaded records created before {cutoff_timestamp}")

        with self._db_lock:
             conn = self._get_db_conn()
             try:
                 cursor = conn.cursor()
                 cursor.execute("""
                     DELETE FROM telemetry_history
                     WHERE uplinked = 1 AND created_at < ?
                 """, (cutoff_timestamp,))
                 conn.commit()
                 deleted_count = cursor.rowcount
                 if deleted_count > 0:
                     logger.info(f"Pruned {deleted_count} old uploaded records.")
             except sqlite3.Error as e:
                 logger.error(f"Database prune_uploaded_records error: {e}")
             finally:
                 conn.close()

    # TODO: Implement background pruning task for uplinked=1 records

    def get_history(self, limit: int = 100):
        """Return the telemetry history from the database.
        If role is 'vehicle', only returns records received within the dashboard_retention_s window.
        Otherwise (for role 'remote' or other), returns the latest records up to the limit.
        Uses 'received_at' for vehicle role filtering, 'recorded_at' for general ordering.
        """
        with self._db_lock:
            conn = self._get_db_conn()
            try:
                cursor = conn.cursor()
                select_cols = "recorded_at, message_type, component_type, component_id, command_id, value_type, value"

                if self.role == 'vehicle':
                    min_received_timestamp = time.time() - self.dashboard_retention_s
                    cursor.execute(f"""
                        SELECT {select_cols} FROM telemetry_history
                        WHERE received_at >= ?
                        ORDER BY recorded_at DESC
                        LIMIT ?
                    """, (min_received_timestamp, limit))
                else: # Remote role or unspecified
                    cursor.execute(f"""
                        SELECT {select_cols} FROM telemetry_history
                        ORDER BY recorded_at DESC
                        LIMIT ?
                    """, (limit,))

                rows = cursor.fetchall()
                # Convert rows to dictionaries and make readable
                readable_history = []
                for row in rows:
                    record_dict = dict(row)
                    # Rename recorded_at to timestamp for GoKartState compatibility
                    record_dict['timestamp'] = record_dict.pop('recorded_at') 
                    state_obj = GoKartState(**record_dict)
                    readable_record = state_to_readable_dict(state_obj, self.protocol)
                    readable_history.append(readable_record)
                return readable_history
            except sqlite3.Error as e:
                logger.error(f"Database history query error: {e}")
                return []
            finally:
                conn.close()

    def get_history_with_pagination(self, limit: int = 100, offset: int = 0):
        """
        Return paginated telemetry history from the database.
        If role is 'vehicle', only includes records received within the dashboard_retention_s window.
        Uses 'received_at' for vehicle role filtering, 'recorded_at' for general ordering.
        """
        with self._db_lock:
            conn = self._get_db_conn()
            try:
                cursor = conn.cursor()
                select_cols = "recorded_at, message_type, component_type, component_id, command_id, value_type, value"

                if self.role == 'vehicle':
                    min_received_timestamp = time.time() - self.dashboard_retention_s
                    cursor.execute(f"""
                        SELECT {select_cols} FROM telemetry_history
                        WHERE received_at >= ?
                        ORDER BY recorded_at DESC
                        LIMIT ? OFFSET ?
                    """, (min_received_timestamp, limit, offset))
                else: # Remote role or unspecified
                    cursor.execute(f"""
                        SELECT {select_cols} FROM telemetry_history
                        ORDER BY recorded_at DESC
                        LIMIT ? OFFSET ?
                    """, (limit, offset))

                rows = cursor.fetchall()
                # Convert rows to dictionaries and make them readable
                readable_history = []
                for row in rows:
                    row_dict = dict(row)
                    row_dict['timestamp'] = row_dict.pop('recorded_at')
                    state_obj = GoKartState(**row_dict)
                    readable_record = state_to_readable_dict(state_obj, self.protocol)
                    # Optionally add other metadata if needed downstream
                    readable_history.append(readable_record)
                return readable_history
            except sqlite3.Error as e:
                logger.error(f"Database paginated history query error: {e}")
                return []
            finally:
                conn.close()
                
    def get_database_stats(self):
        """
        Get statistics about the telemetry database
        
        Returns:
            Dictionary with database statistics
        """
        with self._db_lock:
            conn = self._get_db_conn()
            try:
                cursor = conn.cursor()
                # Get total count
                cursor.execute("SELECT COUNT(*) as total FROM telemetry_history")
                total_count = cursor.fetchone()['total']
                
                # Get earliest and latest timestamps
                cursor.execute("SELECT MIN(timestamp) as earliest, MAX(timestamp) as latest FROM telemetry_history")
                time_range = cursor.fetchone()
                earliest = time_range['earliest'] if time_range else None
                latest = time_range['latest'] if time_range else None
                
                # Get component type distribution
                cursor.execute("""
                    SELECT component_type, COUNT(*) as count 
                    FROM telemetry_history 
                    GROUP BY component_type
                """)
                component_distribution = {}
                for row in cursor.fetchall():
                    comp_type = row['component_type']
                    comp_name = self.protocol.get_component_type_name(comp_type)
                    component_distribution[comp_name] = row['count']
                
                return {
                    'total_records': total_count,
                    'earliest_timestamp': earliest,
                    'latest_timestamp': latest,
                    'component_distribution': component_distribution,
                    'duplicate_skips': self._duplicate_skip_count, # Add the skip count
                    'db_path': self.db_path
                }
            except sqlite3.Error as e:
                logger.error(f"Database stats query error: {e}")
                return {'error': str(e)}
            finally:
                conn.close()
                
    def get_last_update_time(self):
        """
        Get the timestamp of the most recent telemetry update
        
        Returns:
            Timestamp of the most recent update or None if no updates
        """
        return self.last_update_time

    # Optional: Method to get state for a specific component
    def get_component_state(self, component_type_name: str, component_id_name: str):
        """Get the most recent state for a specific component from the DB (based on recorded_at)."""
        comp_type = self.protocol.get_component_type(component_type_name.upper())
        comp_id = self.protocol.get_component_id(component_type_name.lower(), component_id_name.upper())

        if comp_type is None or comp_id is None:
            logger.warning(f"Could not find component type/id for {component_type_name}/{component_id_name}")
            return None

        with self._db_lock:
            conn = self._get_db_conn()
            try:
                cursor = conn.cursor()
                select_cols = "recorded_at, message_type, component_type, component_id, command_id, value_type, value"
                cursor.execute(f"""
                    SELECT {select_cols} FROM telemetry_history
                    WHERE component_type = ? AND component_id = ?
                    ORDER BY recorded_at DESC -- Get latest based on original event time
                    LIMIT 1
                """, (comp_type, comp_id))
                row = cursor.fetchone()
                if row:
                    # Convert row to a readable dictionary format
                    row_dict = dict(row)
                    row_dict['timestamp'] = row_dict.pop('recorded_at')
                    state_obj = GoKartState(**row_dict)
                    readable_state = state_to_readable_dict(state_obj, self.protocol)
                    return readable_state
                else:
                    return None # No state found for this component
            except sqlite3.Error as e:
                logger.error(f"Database component state query error for {component_type_name}/{component_id_name}: {e}")
                return None
            finally:
                conn.close()


