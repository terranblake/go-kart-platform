""" Optimized Telemetry Storage using Redis Streams with Field Names """

import logging
import time
from typing import List, Dict, Any, Optional, Union

import redis
from redis.exceptions import RedisError

from shared.lib.python.telemetry.store import TelemetryStore, state_to_readable_dict
from shared.lib.python.telemetry.state import GoKartState
from shared.lib.python.can.protocol_registry import ProtocolRegistry

logger = logging.getLogger(__name__)

class PersistentTelemetryStore(TelemetryStore):
    def __init__(self, protocol: ProtocolRegistry,
                 local_redis_config: dict = None,
                 remote_redis_config: dict = None,
                 max_history_records: int = 10000,
                 role: str = 'vehicle',
                 retention_seconds: int = 600,  # Default 10 minutes
                 stream_key_prefix: str = "telemetry"):
        """
        Initialize Redis-based telemetry store using Redis Streams with field names.
        
        Args:
            protocol: Protocol registry for telemetry interpretation
            local_redis_config: Configuration for local Redis (host, port, db)
            remote_redis_config: Configuration for remote Redis (host, port, db)
            max_history_records: Maximum number of records to keep in stream
            role: 'vehicle' or 'remote'
            retention_seconds: How long to keep data for dashboard (seconds)
            stream_key_prefix: Prefix for Redis keys
        """
        # Pass protocol and memory history limit to base class
        super().__init__(protocol, limit=1000)
        
        # Set up default configurations if not provided
        self.local_redis_config = local_redis_config or {
            'host': 'localhost',
            'port': 6379,
            'db': 0
        }
        
        self.remote_redis_config = remote_redis_config or {
            'host': 'remote-server',
            'port': 6379,
            'db': 0
        }
        
        # Configuration
        self.stream_key = f"{stream_key_prefix}:stream"
        self.component_key_pattern = f"{stream_key_prefix}:component:{{}}:{{}}"
        self.node_key_pattern = f"{stream_key_prefix}:node:{{}}"
        self.max_history_records = max_history_records
        self.role = role
        self.retention_seconds = retention_seconds
        
        # Redis connection pools for thread safety
        self.local_redis_pool = redis.ConnectionPool(
            **self.local_redis_config,
            max_connections=10,
            socket_timeout=2,
            socket_keepalive=True,
            decode_responses=True  # Auto-decode to strings
        )
        
        # Remote Redis connection pool (if role is 'vehicle')
        if self.role == 'vehicle' and self.remote_redis_config:
            self.remote_redis_pool = redis.ConnectionPool(
                **self.remote_redis_config,
                max_connections=5,
                socket_timeout=5,
                socket_connect_timeout=5,
                retry_on_timeout=True,
                decode_responses=True
            )
        else:
            self.remote_redis_pool = None
        
        # Initialize Redis
        self._init_redis()

    def _get_local_redis(self):
        """Get a Redis connection from the local pool"""
        return redis.Redis(connection_pool=self.local_redis_pool)

    def _get_remote_redis(self):
        """Get a Redis connection from the remote pool if available"""
        if self.remote_redis_pool:
            return redis.Redis(connection_pool=self.remote_redis_pool)
        return None

    def _init_redis(self):
        """Initialize Redis data structures and optimal settings"""
        logger.info(f"Initializing Redis connections: local={self.local_redis_config['host']}:{self.local_redis_config['port']}")
        
        try:
            # Configure local Redis for optimal performance on Pi Zero W
            r_local = self._get_local_redis()
            
            # Set memory policy
            r_local.config_set('maxmemory-policy', 'allkeys-lru')
            
            # Disable AOF persistence for higher throughput
            r_local.config_set('appendonly', 'no')
            
            # Disable RDB persistence
            r_local.config_set('save', '')
            
            # Create stream if it doesn't exist (will be created implicitly with XADD)
            logger.info(f"Using stream key: {self.stream_key}")
            
            # Configure remote Redis (if available and vehicle role)
            r_remote = self._get_remote_redis()
            if r_remote:
                logger.info(f"Remote Redis connection established to {self.remote_redis_config['host']}:{self.remote_redis_config['port']}")
            
            logger.info("Redis successfully initialized")
        except RedisError as e:
            logger.error(f"Redis initialization error: {e}")

    def update_state(self, state: GoKartState, node_id: str = "default"):
        """
        Update the current state (in memory) and add to Redis stream with full field names.
        Also replicates to remote Redis if in vehicle role.
        
        Args:
            state: The telemetry state to store
            node_id: ID of the node sending the telemetry
            
        Returns:
            Dictionary with state information
        """
        # Update in-memory state (from base class)
        super().update_state(state)
        state_dict = state.to_dict()
        
        # Calculate current timestamp
        received_ts = time.time()
        
        # Create record for Redis stream with descriptive field names
        # Ensure all values are strings, ints, floats, or bytes for Redis
        record = {
            'timestamp': str(state.timestamp),
            'node_id': node_id,
            'received_at': str(received_ts),
            'message_type': str(state.message_type),
            'component_type': str(state.component_type),
            'component_id': str(state.component_id),
            'command_id': str(state.command_id),
            'value_type': str(state.value_type),
            'value': str(state.value) # Value must be suitable type
        }
        
        actual_stream_id = None
        new_message_count = None

        try:
            r_local = self._get_local_redis()
            
            # --- Pipeline Part 1: Add stream entry, component/node hashes (partial), expirations ---
            pipe = r_local.pipeline(transaction=False)
            
            # 1. Add to main stream (Command 0 in pipeline results)
            pipe.xadd(
                self.stream_key,
                record,
                maxlen=self.max_history_records,
                approximate=True
            )
            
            # 2. Set expiration on the stream (Command 1)
            pipe.expire(self.stream_key, self.retention_seconds)
            
            # 3. Update component state hash (Partial - without last_stream_id) (Command 2)
            component_key = self.component_key_pattern.format(state.component_type, state.component_id)
            # Ensure all values are primitive types
            component_data_partial = {
                'last_value': str(state.value), # Ensure string
                'last_timestamp': state.timestamp, # Float OK
                'last_received': received_ts, # Float OK
                'message_type': state.message_type, # Int OK
                'command_id': state.command_id, # Int OK
                'value_type': state.value_type, # Int OK
                # 'last_stream_id': Will be added later
            }
            pipe.hset(component_key, mapping=component_data_partial)
            
            # 4. Set expiration on component hash (Command 3)
            pipe.expire(component_key, self.retention_seconds)
            
            # 5. Update node status hash (Increment count, set others) (Command 4, 5)
            node_key = self.node_key_pattern.format(node_id)
            pipe.hincrby(node_key, 'message_count', 1) # Increment count (Command 4)
            node_data_partial = {
                'last_update': received_ts,
                'last_latency': received_ts - state.timestamp,
                # 'message_count': Will be added/updated by hincrby
            }
            pipe.hset(node_key, mapping=node_data_partial) # Set other fields (Command 5)
            
            # 6. Set expiration on node hash (Command 6)
            pipe.expire(node_key, self.retention_seconds)
            
            # Execute local pipeline
            results = pipe.execute()
            
            # --- Post-Pipeline Updates --- 
            # Retrieve results from pipeline execution
            actual_stream_id = results[0] # Result of XADD
            new_message_count = results[4] # Result of HINCRBY
            
            # Now update component and node hashes with the actual stream ID and count
            # These are separate commands, not pipelined with the first batch
            if actual_stream_id:
                 r_local.hset(component_key, 'last_stream_id', actual_stream_id)
            # Note: new_message_count is already set by HINCRBY in the pipeline
            # If node_data_partial had other fields relying on count, update them here

            logger.debug(f"Added telemetry record {actual_stream_id} for {component_key}")

            # --- Remote Replication (if applicable) --- 
            if self.role == 'vehicle':
                r_remote = self._get_remote_redis()
                if r_remote:
                    try:
                        # Build complete data dictionaries now that we have the stream ID
                        component_data_complete = component_data_partial.copy()
                        component_data_complete['last_stream_id'] = actual_stream_id
                        
                        node_data_complete = node_data_partial.copy()
                        node_data_complete['message_count'] = new_message_count

                        # Use a separate pipeline for remote Redis
                        remote_pipe = r_remote.pipeline(transaction=False)
                        
                        # Add to remote stream (no trimming)
                        remote_pipe.xadd(self.stream_key, record) 
                        
                        # Update component and node data on remote
                        remote_pipe.hset(component_key, mapping=component_data_complete)
                        remote_pipe.hset(node_key, mapping=node_data_complete)
                        
                        # No expiration on remote Redis - data preserved long-term
                        
                        # Execute remote pipeline
                        remote_pipe.execute()
                    except RedisError as e:
                        logger.error(f"Error adding to remote Redis: {e}")
            
        except RedisError as e:
            logger.error(f"Redis update_state error: {e}")
        except IndexError:
             logger.error("Redis pipeline result indexing error. Mismatch between commands and results?", exc_info=True)
        
        return state_dict

    def get_history(self, limit: int = 100):
        """
        Return the telemetry history from Redis using field names.
        
        Args:
            limit: Maximum number of records to return
            
        Returns:
            List of telemetry records
        """
        try:
            r_local = self._get_local_redis()
            
            # Get records from stream (newest first)
            stream_entries = r_local.xrevrange(self.stream_key, count=limit)
            
            # Convert to the format expected by the application
            readable_history = []
            for entry_id, entry in stream_entries:
                # Create GoKartState object from record
                state_obj = GoKartState(
                    timestamp=float(entry.get('timestamp', 0)),
                    message_type=int(entry.get('message_type', 0)),
                    component_type=int(entry.get('component_type', 0)),
                    component_id=int(entry.get('component_id', 0)),
                    command_id=int(entry.get('command_id', 0)),
                    value_type=int(entry.get('value_type', 0)),
                    value=int(entry.get('value', 0))
                )
                
                # Convert to readable format
                readable_record = state_to_readable_dict(state_obj, self.protocol)
                
                # Add additional info
                readable_record['node_id'] = entry.get('node_id', 'unknown')
                readable_record['received_at'] = float(entry.get('received_at', 0))
                readable_record['latency_ms'] = (float(entry.get('received_at', 0)) - float(entry.get('timestamp', 0))) * 1000
                readable_record['stream_id'] = entry_id
                
                readable_history.append(readable_record)
                    
            return readable_history
            
        except RedisError as e:
            logger.error(f"Redis get_history error: {e}")
            return []

    def get_history_with_pagination(self, 
                                   limit: int = 100, 
                                   offset: int = 0, 
                                   start_time: Optional[float] = None,
                                   end_time: Optional[float] = None,
                                   component_type: Optional[int] = None,
                                   component_id: Optional[int] = None,
                                   command_id: Optional[int] = None):
        """
        Return paginated telemetry history with advanced filtering options.
        
        Args:
            limit: Maximum number of records to return
            offset: Number of records to skip
            start_time: Filter records after this timestamp
            end_time: Filter records before this timestamp
            component_type: Filter by component type
            component_id: Filter by component ID
            command_id: Filter by command ID
            
        Returns:
            List of filtered telemetry records
        """
        try:
            r_local = self._get_local_redis()
            
            # Convert timestamps to milliseconds for Redis streams
            min_id = "-"  # Start from oldest
            max_id = "+"  # Up to newest
            
            if start_time:
                min_id = f"{int(start_time * 1000)}-0"
                
            if end_time:
                max_id = f"{int(end_time * 1000)}-0"
            
            # Determine if we need component filtering
            need_filtering = component_type is not None or component_id is not None or command_id is not None
            
            # If using component filtering, we need to fetch more records
            fetch_count = limit + offset
            if need_filtering:
                fetch_count = fetch_count * 3  # Fetch extra to account for filtering
                
            # Get records from stream within time range
            stream_entries = r_local.xrevrange(
                self.stream_key, 
                max=max_id,
                min=min_id,
                count=fetch_count
            )
            
            # Filter by component if needed
            filtered_entries = []
            for entry_id, entry in stream_entries:
                # Apply component filters if specified
                if component_type is not None and int(entry.get('component_type', -1)) != component_type:
                    continue
                    
                if component_id is not None and int(entry.get('component_id', -1)) != component_id:
                    continue
                    
                if command_id is not None and int(entry.get('command_id', -1)) != command_id:
                    continue
                
                filtered_entries.append((entry_id, entry))
            
            # Apply pagination
            paginated_entries = filtered_entries[offset:offset + limit]
            
            # Convert to the format expected by the application
            readable_history = []
            for entry_id, entry in paginated_entries:
                # Create GoKartState object
                state_obj = GoKartState(
                    timestamp=float(entry.get('timestamp', 0)),
                    message_type=int(entry.get('message_type', 0)),
                    component_type=int(entry.get('component_type', 0)),
                    component_id=int(entry.get('component_id', 0)),
                    command_id=int(entry.get('command_id', 0)),
                    value_type=int(entry.get('value_type', 0)),
                    value=int(entry.get('value', 0))
                )
                
                # Convert to readable format
                readable_record = state_to_readable_dict(state_obj, self.protocol)
                
                # Add additional info
                readable_record['node_id'] = entry.get('node_id', 'unknown')
                readable_record['received_at'] = float(entry.get('received_at', 0))
                readable_record['latency_ms'] = (float(entry.get('received_at', 0)) - float(entry.get('timestamp', 0))) * 1000
                readable_record['stream_id'] = entry_id
                
                readable_history.append(readable_record)
                
            return readable_history
            
        except RedisError as e:
            logger.error(f"Redis get_history_with_pagination error: {e}")
            return []

    def get_component_state(self, component_type_name: str, component_id_name: str):
        """
        Get the most recent state for a specific component using the dedicated hash.
        Uses component hash for O(1) lookup instead of scanning the stream.
        
        Args:
            component_type_name: Name of the component type
            component_id_name: Name of the component ID
            
        Returns:
            Component state dictionary or None if not found
        """
        comp_type = self.protocol.get_component_type(component_type_name.upper())
        comp_id = self.protocol.get_component_id(component_type_name.lower(), component_id_name.upper())

        if comp_type is None or comp_id is None:
            logger.warning(f"Could not find component type/id for {component_type_name}/{component_id_name}")
            return None

        try:
            r_local = self._get_local_redis()
            
            # Get component hash directly - O(1) operation
            component_key = self.component_key_pattern.format(comp_type, comp_id)
            component_data = r_local.hgetall(component_key)
            
            if not component_data:
                return None
                
            # Create a GoKartState from the hash data
            state_obj = GoKartState(
                timestamp=float(component_data.get('last_timestamp', 0)),
                message_type=int(component_data.get('message_type', 0)),
                component_type=comp_type,
                component_id=comp_id,
                command_id=int(component_data.get('command_id', 0)),
                value_type=int(component_data.get('value_type', 0)),
                value=int(component_data.get('last_value', 0))
            )
            
            # Convert to readable format
            readable_state = state_to_readable_dict(state_obj, self.protocol)
            
            # Add additional information
            readable_state['last_received'] = float(component_data.get('last_received', 0))
            
            return readable_state
            
        except RedisError as e:
            logger.error(f"Redis get_component_state error: {e}")
            return None

    def get_component_history(self, 
                             component_type: int, 
                             component_id: int, 
                             command_id: Optional[int] = None,
                             start_time: Optional[float] = None,
                             end_time: Optional[float] = None,
                             limit: int = 100):
        """
        Get historical values for a specific component over time.
        
        Args:
            component_type: The component type number
            component_id: The component ID number
            command_id: Optional command ID to filter by
            start_time: Start time (default: retention window)
            end_time: End time (default: now)
            limit: Maximum number of points to return
            
        Returns:
            List of component states over time
        """
        try:
            r_local = self._get_local_redis()
            
            # Convert timestamps to milliseconds for Redis streams
            min_id = "-"
            max_id = "+"
            
            if start_time:
                min_id = f"{int(start_time * 1000)}-0"
                
            if end_time:
                max_id = f"{int(end_time * 1000)}-0"
            
            # Create filters for the query
            match_filters = {
                'component_type': str(component_type),
                'component_id': str(component_id)
            }
            
            if command_id is not None:
                match_filters['command_id'] = str(command_id)
                
            # Get matching records from stream
            stream_entries = r_local.xrevrange(
                self.stream_key,
                min=min_id,
                max=max_id,
                count=limit
            )
            
            # Filter entries manually (would be nice if Redis supported field filtering)
            filtered_entries = []
            for entry_id, entry in stream_entries:
                # Check if all match_filters are satisfied
                if all(entry.get(k) == v for k, v in match_filters.items()):
                    filtered_entries.append((entry_id, entry))
            
            # Convert to readable format
            readable_history = []
            for entry_id, entry in filtered_entries:
                # Create GoKartState object
                state_obj = GoKartState(
                    timestamp=float(entry.get('timestamp', 0)),
                    message_type=int(entry.get('message_type', 0)),
                    component_type=component_type,
                    component_id=component_id,
                    command_id=int(entry.get('command_id', 0)),
                    value_type=int(entry.get('value_type', 0)),
                    value=int(entry.get('value', 0))
                )
                
                # Convert to readable format
                readable_record = state_to_readable_dict(state_obj, self.protocol)
                readable_record['stream_id'] = entry_id
                readable_history.append(readable_record)
                
            return readable_history
            
        except RedisError as e:
            logger.error(f"Redis get_component_history error: {e}")
            return []

    def get_database_stats(self):
        """
        Get statistics about the Redis telemetry store
        
        Returns:
            Dictionary with statistics
        """
        try:
            r_local = self._get_local_redis()
            
            # Get stream length with XLEN (O(1) operation)
            stream_length = r_local.xlen(self.stream_key)
            
            # Get first and last entry timestamps efficiently
            first_timestamp = None
            last_timestamp = None
            
            if stream_length > 0:
                # Get oldest entry ID
                first_entries = r_local.xrange(self.stream_key, count=1)
                if first_entries:
                    first_id, _ = first_entries[0]
                    first_timestamp = float(first_id.split('-')[0]) / 1000
                
                # Get newest entry ID
                last_entries = r_local.xrevrange(self.stream_key, count=1)
                if last_entries:
                    last_id, _ = last_entries[0]
                    last_timestamp = float(last_id.split('-')[0]) / 1000
            
            # Count active components and nodes using key patterns
            component_keys = r_local.keys(self.component_key_pattern.format('*', '*'))
            node_keys = r_local.keys(self.node_key_pattern.format('*'))
            
            # Get memory usage
            try:
                memory_usage = r_local.memory_usage(self.stream_key)
            except RedisError:
                memory_usage = None
            
            # Get TTL for stream
            ttl = r_local.ttl(self.stream_key)
            
            return {
                'total_records': stream_length,
                'active_components': len(component_keys),
                'active_nodes': len(node_keys),
                'earliest_timestamp': first_timestamp,
                'latest_timestamp': last_timestamp,
                'memory_usage_bytes': memory_usage,
                'stream_key': self.stream_key,
                'local_host': self.local_redis_config['host'],
                'local_port': self.local_redis_config['port'],
                'remote_host': self.remote_redis_config['host'] if self.remote_redis_pool else None,
                'remote_port': self.remote_redis_config['port'] if self.remote_redis_pool else None,
                'retention_seconds': self.retention_seconds,
                'ttl_seconds': ttl
            }
            
        except RedisError as e:
            logger.error(f"Redis get_database_stats error: {e}")
            return {'error': str(e)}

    def get_last_update_time(self):
        """
        Get the timestamp of the most recent telemetry update
        
        Returns:
            Timestamp of the most recent update or None if no updates
        """
        return self.last_update_time

    def get_active_nodes(self):
        """
        Get list of active nodes and their basic statistics
        
        Returns:
            Dictionary mapping node_id to statistics
        """
        try:
            r_local = self._get_local_redis()
            
            # Get all node keys
            node_keys = r_local.keys(self.node_key_pattern.format('*'))
            
            # Extract node IDs from keys
            node_ids = [key.split(':')[-1] for key in node_keys]
            
            # Get stats for each node
            node_stats = {}
            for node_id in node_ids:
                node_key = self.node_key_pattern.format(node_id)
                node_data = r_local.hgetall(node_key)
                
                if node_data:
                    node_stats[node_id] = {
                        'last_update': float(node_data.get('last_update', 0)),
                        'last_latency': float(node_data.get('last_latency', 0)),
                        'message_count': int(node_data.get('message_count', 0)),
                        'seconds_since_update': time.time() - float(node_data.get('last_update', 0))
                    }
            
            return node_stats
            
        except RedisError as e:
            logger.error(f"Redis get_active_nodes error: {e}")
            return {}

    def get_active_components(self):
        """
        Get list of active components and their basic info
        
        Returns:
            List of dictionaries with component information
        """
        try:
            r_local = self._get_local_redis()
            
            # Get all component keys
            component_keys = r_local.keys(self.component_key_pattern.format('*', '*'))
            
            # Extract component types and IDs from keys
            components = []
            for key in component_keys:
                parts = key.split(':')
                if len(parts) >= 4:
                    try:
                        comp_type = int(parts[-2])
                        comp_id = int(parts[-1])
                        
                        # Get type and ID names
                        comp_type_name = self.protocol.get_component_type_name(comp_type) or f"Type_{comp_type}"
                        comp_id_name = self.protocol.get_component_id_name(comp_type, comp_id) or f"ID_{comp_id}"
                        
                        components.append({
                            'component_type': comp_type,
                            'component_id': comp_id,
                            'component_type_name': comp_type_name,
                            'component_id_name': comp_id_name,
                            'key': key
                        })
                    except (ValueError, IndexError):
                        continue
            
            # Get detailed info for each component
            for comp in components:
                comp_data = r_local.hgetall(comp['key'])
                
                if comp_data:
                    comp.update({
                        'last_value': int(comp_data.get('last_value', 0)),
                        'last_timestamp': float(comp_data.get('last_timestamp', 0)),
                        'last_received': float(comp_data.get('last_received', 0)),
                        'seconds_since_update': time.time() - float(comp_data.get('last_timestamp', 0))
                    })
            
            return components
            
        except RedisError as e:
            logger.error(f"Redis get_active_components error: {e}")
            return []