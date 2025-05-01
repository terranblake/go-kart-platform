""" Telemetry Storage Logic (including persistence) """

import sqlite3
import logging
import time
import threading
from shared.lib.python.telemetry.store import TelemetryStore, state_to_readable_dict
from shared.lib.python.telemetry.state import GoKartState
from shared.lib.python.can.protocol_registry import ProtocolRegistry

logger = logging.getLogger(__name__)

class PersistentTelemetryStore(TelemetryStore):
    def __init__(self, protocol: ProtocolRegistry, db_path: str = "telemetry.db", max_history_records: int = 10000):
        super().__init__(protocol)
        self.db_path = db_path
        self.max_history_records = max_history_records
        self._db_lock = threading.Lock() # Use lock for thread safety
        self._init_db()
        # TODO: Load recent history from DB on startup?

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
                cursor.execute("""
                    CREATE TABLE IF NOT EXISTS telemetry_history (
                        timestamp REAL PRIMARY KEY,
                        message_type INTEGER,
                        component_type INTEGER,
                        component_id INTEGER,
                        command_id INTEGER,
                        value_type INTEGER,
                        value INTEGER
                    )
                """)
                conn.commit()
                logger.info("Database initialized successfully.")
            except sqlite3.Error as e:
                logger.error(f"Database initialization error: {e}")
            finally:
                conn.close()

    def update_state(self, state: GoKartState):
        """Update the current state (in memory) and add to DB history"""
        # Update in-memory state (from base class)
        super().update_state(state) 
        state_dict = state.to_dict() # Use the dict from base class
        
        # Persist to DB
        with self._db_lock:
            conn = self._get_db_conn()
            try:
                cursor = conn.cursor()
                cursor.execute("""
                    INSERT INTO telemetry_history (timestamp, message_type, component_type, 
                                                component_id, command_id, value_type, value)
                    VALUES (?, ?, ?, ?, ?, ?, ?)
                """, (
                    state.timestamp,
                    state.message_type,
                    state.component_type,
                    state.component_id,
                    state.command_id,
                    state.value_type,
                    state.value
                ))
                conn.commit()
                
                # Prune old records
                cursor.execute(f"""
                    DELETE FROM telemetry_history
                    WHERE timestamp NOT IN (
                        SELECT timestamp
                        FROM telemetry_history
                        ORDER BY timestamp DESC
                        LIMIT {self.max_history_records}
                    )
                """)
                conn.commit()

            except sqlite3.IntegrityError:
                 logger.warning(f"Duplicate timestamp {state.timestamp}, skipping DB insert.")
            except sqlite3.Error as e:
                logger.error(f"Database update error: {e}")
            finally:
                conn.close()
                
        return state_dict # Return dict consistent with base class

    def get_history(self, limit: int = 100):
        """Return the telemetry history from the database"""
        with self._db_lock:
            conn = self._get_db_conn()
            try:
                cursor = conn.cursor()
                cursor.execute("""
                    SELECT * FROM telemetry_history
                    ORDER BY timestamp DESC
                    LIMIT ?
                """, (limit,))
                rows = cursor.fetchall()
                # Convert rows to dictionaries
                history = [dict(row) for row in rows]
                # Convert to readable format
                readable_history = []
                for record in history:
                    state_obj = GoKartState(**record)
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
        Return paginated telemetry history from the database
        
        Args:
            limit: Maximum number of records to return
            offset: Number of records to skip
            
        Returns:
            List of telemetry records in readable format
        """
        with self._db_lock:
            conn = self._get_db_conn()
            try:
                cursor = conn.cursor()
                cursor.execute("""
                    SELECT * FROM telemetry_history
                    ORDER BY timestamp DESC
                    LIMIT ? OFFSET ?
                """, (limit, offset))
                rows = cursor.fetchall()
                # Convert rows to dictionaries and make them readable
                readable_history = []
                for row in rows:
                    row_dict = dict(row)
                    state_obj = GoKartState(**row_dict)
                    readable_record = state_to_readable_dict(state_obj, self.protocol)
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
        """Get the most recent state for a specific component from the DB."""
        comp_type = self.protocol.get_component_type(component_type_name.upper())
        comp_id = self.protocol.get_component_id(component_type_name.lower(), component_id_name.upper())

        if comp_type is None or comp_id is None:
            logger.warning(f"Could not find component type/id for {component_type_name}/{component_id_name}")
            return None

        with self._db_lock:
            conn = self._get_db_conn()
            try:
                cursor = conn.cursor()
                cursor.execute("""
                    SELECT * FROM telemetry_history
                    WHERE component_type = ? AND component_id = ?
                    ORDER BY timestamp DESC
                    LIMIT 1
                """, (comp_type, comp_id))
                row = cursor.fetchone()
                if row:
                    # Convert row to a readable dictionary format
                    state_obj = GoKartState(**dict(row))
                    # Use the refactored static method for conversion
                    readable_state = state_to_readable_dict(state_obj, self.protocol)
                    return readable_state
                else:
                    return None # No state found for this component
            except sqlite3.Error as e:
                logger.error(f"Database component state query error for {component_type_name}/{component_id_name}: {e}")
                return None
            finally:
                conn.close()


