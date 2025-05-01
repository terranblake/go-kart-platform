from .state import GoKartState
from shared.lib.python.can.protocol_registry import ProtocolRegistry
import time
import logging

logger = logging.getLogger(__name__)

def state_to_readable_dict(state: GoKartState, protocol: ProtocolRegistry) -> dict:
    """Convert a GoKartState object to a readable dictionary format."""
    readable_dict = {}
    state_dict = state.to_dict() # Get base dictionary
    for key, value in state_dict.items():
        try:
            if value is None: # Handle potential None values
                readable_dict[key] = None
                continue
                
            if key == 'message_type':
                readable_dict[key] = protocol.get_message_type_name(value)
            elif key == 'component_type':
                readable_dict[key] = protocol.get_component_type_name(value)
            elif key == 'component_id':
                # Need component_type which should be present in the state object
                component_type_val = state_dict.get('component_type')
                if component_type_val is not None:
                    readable_dict[key] = protocol.get_component_id_name(component_type_val, value)
                else:
                     readable_dict[key] = f"UnknownComponentId({value})"
            elif key == 'command_id':
                 # Need component_type which should be present in the state object
                component_type_val = state_dict.get('component_type')
                if component_type_val is not None:
                    readable_dict[key] = protocol.get_command_name(component_type_val, value)
                else:
                    readable_dict[key] = f"UnknownCommandId({value})"
            elif key == 'value_type':
                readable_dict[key] = protocol.get_value_type_name(value)
            elif key == 'value':
                # Need component_type and command_id
                component_type_val = state_dict.get('component_type')
                command_id_val = state_dict.get('command_id')
                if component_type_val is not None and command_id_val is not None:
                     readable_dict[key] = protocol.get_command_value_name(component_type_val, command_id_val, value)
                else:
                    readable_dict[key] = f"UnknownValue({value})"
            else:
                readable_dict[key] = value
        except Exception as e:
             logger.error(f"Error converting key '{key}' with value '{value}': {e}", exc_info=True)
             readable_dict[key] = f"ConversionError({value})"
    return readable_dict

class TelemetryStore:
    def __init__(self, protocol: ProtocolRegistry, limit=100):
        self.state = GoKartState() # Holds the *very last* message received
        self.protocol = protocol
        self.limit = limit

        # even though we use persistent store, we still need to keep a history for the dashboard
        self.history = []
        self.last_update_time = time.time()

    def get_current_state(self, readable=False):
        """Return the current state (last message) as a dictionary."""
        state_dict = self.state.to_dict()
        state_dict['timestamp'] = self.last_update_time # Ensure latest timestamp
        if readable:
            return state_to_readable_dict(self.state, self.protocol)
        else:
            return state_dict

    def get_history(self, limit=100):
        """
        Stub method for backward compatibility with dashboard.
        In the base class, this returns an empty list.
        PersistentTelemetryStore overrides this with DB access.
        Keep this data for 
        """
        # get limit max self.limit
        _limit = min(limit, self.limit)
        return self.history[-_limit:]

    def update_state(self, state: GoKartState):
        """Update the current state (last message received)."""
        self.state = state
        self.last_update_time = state.timestamp
        state_dict = state.to_dict()
        self.history.append(state_dict)
        if len(self.history) > self.limit:
            self.history.pop(0)
        return state_dict # Return the raw state object instead? Or None?