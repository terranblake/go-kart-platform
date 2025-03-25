"""
Go-Kart Dashboard Server - Main entry point
"""

import threading
import logging
from api.endpoints import app, socketio, send_updates
# Configure logging
logging.basicConfig(level=logging.INFO,
                   format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

if __name__ == "__main__":
    logger.info("Starting Go-Kart Dashboard Server")
    
    # Start background thread for updates
    thread = threading.Thread(target=send_updates, daemon=True)
    thread.start()
    logger.info("Started state update thread")
    
    try:
        # Start the web server
        logger.info("Starting web server on port 5000")
        socketio.run(app, host='0.0.0.0', port=5000, allow_unsafe_werkzeug=True)
    except KeyboardInterrupt:
        logger.info("Server shutdown requested by user")
    except Exception as e:
        logger.error(f"Error running server: {e}")
    finally:
        logger.info("Server shutdown complete")