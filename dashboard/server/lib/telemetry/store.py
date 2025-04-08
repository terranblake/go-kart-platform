from lib.telemetry.state import GoKartState
import time

class TelemetryStore:
    def __init__(self):     
        self.state = GoKartState()
        self.history = []
        self.last_update_time = time.time()

    def get_current_state(self):
        """Return the current state as a dictionary with updated timestamp"""
        state_dict = self.state.to_dict()
        # Ensure timestamps are always updated when requested
        state_dict['timestamp'] = self.last_update_time
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