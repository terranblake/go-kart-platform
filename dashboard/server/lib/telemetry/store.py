from lib.telemetry.state import GoKartState
from lib.can.protocol_registry import ProtocolRegistry
import time

class TelemetryStore:
    def __init__(self, protocol: ProtocolRegistry):     
        self.state = GoKartState()
        self.protocol = protocol
        self.history = []
        self.last_update_time = time.time()

    def get_current_state(self, readable=False):
        """Return the current state as a dictionary with updated timestamp"""
        state_dict = self.state.to_dict()
        # Ensure timestamps are always updated when requested
        state_dict['timestamp'] = self.last_update_time
        if readable:
            # convert the state_dict to a readable format
            state_dict = self.to_readable_dict()
        return state_dict

    def get_history(self):
        """Return the telemetry history"""
        return self.history
    
    def update_state(self, state: GoKartState):
        """Update the current state and add to history"""
        self.state = state
        self.last_update_time = state.timestamp
        state_dict = state.to_dict()
        self.history.append(state_dict)

        # limit history to 100 entries
        if len(self.history) > 100:
            self.history.pop(0)
        
        return state_dict
    
    def to_readable_dict(self):
        """Convert the state to a readable format"""
        # use the protocol to convert the state_dict to a readable format
        readable_dict = {}
        for key, value in self.state.to_dict().items():
            if key == 'message_type':
                readable_dict[key] = self.protocol.get_message_type_name(value)
            elif key == 'component_type':
                readable_dict[key] = self.protocol.get_component_type_name(value)
            elif key == 'component_id':
                readable_dict[key] = self.protocol.get_component_id_name(self.state.component_type, value)
            elif key == 'command_id':
                readable_dict[key] = self.protocol.get_command_name(self.state.component_type, value)
            elif key == 'value_type':
                readable_dict[key] = self.protocol.get_value_type_name(value)
            elif key == 'value':
                readable_dict[key] = self.protocol.get_command_value_name(self.state.component_type, self.state.command_id, value)
            else:
                readable_dict[key] = value
        return readable_dict
