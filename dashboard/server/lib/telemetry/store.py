from lib.telemetry.state import GoKartState

class TelemetryStore:
    def __init__(self):     
        self.state = GoKartState()
        self.history = []

    def get_current_state(self):
        return self.state.to_dict()

    def get_history(self):
        return self.history
    
    def update_state(self, state: GoKartState):
        self.state = state
        self.history.append(state.to_dict())

        # limit history to 100 entries
        if len(self.history) > 100:
            self.history.pop(0)