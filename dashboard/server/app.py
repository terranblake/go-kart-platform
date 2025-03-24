import os
import threading
from api.endpoints import app, socketio, command_generator, send_updates

if __name__ == "__main__":
    # Start update threads for SocketIO
    update_thread = threading.Thread(target=send_updates, daemon=True)
    update_thread.start()
    
    try:
        # Ensure network is up
        try:
            os.system('sudo ip link set can0 up type can bitrate 500000')
        except:
            print('CAN network check failed')
            exit(1)
            
        # Start the server
        socketio.run(app, host='0.0.0.0', port=5000, debug=False)
    finally:
        print('socketio server shutdown')