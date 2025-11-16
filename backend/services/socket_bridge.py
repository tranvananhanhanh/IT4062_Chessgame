import socket
import json
import threading
import time
from typing import Optional, Dict, Any

class CSocketBridge:
    """
    Bridge class to communicate with C TCP server
    Handles connection management and command sending
    """

    def __init__(self, host: str = 'localhost', port: int = 8888):
        self.host = host
        self.port = port
        self.socket: Optional[socket.socket] = None
        self.connected = False
        self.lock = threading.Lock()

    def connect(self) -> bool:
        """Establish connection to C server"""
        try:
            with self.lock:
                if self.connected:
                    return True

                self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.socket.settimeout(10.0)  # 10 second timeout
                self.socket.connect((self.host, self.port))
                self.connected = True

                # Read welcome message
                welcome = self._receive_response()
                if welcome:
                    print(f"Connected to C Server: {welcome}")
                    return True
                else:
                    self._close_connection()
                    return False

        except Exception as e:
            print(f"Failed to connect to C server: {e}")
            self._close_connection()
            return False

    def disconnect(self):
        """Close connection to C server"""
        with self.lock:
            self._close_connection()

    def send_command(self, command: str) -> Optional[str]:
        """
        Send command to C server and get response
        Returns response string or None if failed
        """
        if not self.connected:
            if not self.connect():
                return None

        try:
            with self.lock:
                # Send command with newline
                self.socket.send(f"{command}\n".encode())

                # Receive response
                response = self._receive_response()
                return response

        except Exception as e:
            print(f"Error sending command '{command}': {e}")
            self._close_connection()
            return None

    def _receive_response(self) -> Optional[str]:
        """Receive response from C server"""
        try:
            data = self.socket.recv(8192).decode().strip()
            return data
        except socket.timeout:
            print("Timeout receiving response from C server")
            return "ERROR|Timeout"
        except Exception as e:
            print(f"Error receiving response: {e}")
            return None

    def _close_connection(self):
        """Internal method to close socket connection"""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        self.socket = None
        self.connected = False

    def is_connected(self) -> bool:
        """Check if connected to C server"""
        return self.connected

# Global instance
c_bridge = CSocketBridge()

def get_c_bridge() -> CSocketBridge:
    """Get the global C socket bridge instance"""
    return c_bridge