import socket
import threading
from typing import Optional

class CSocketBridge:
    """
    Bridge class to communicate with C TCP server
    Handles persistent connection management
    """

    def __init__(self, host: str = 'localhost', port: int = 8888):
        self.host = host
        self.port = port
        self.socket: Optional[socket.socket] = None
        self.connected = False
        self.lock = threading.Lock()
        
        # Auto-connect on initialization
        self._connect()

    def _connect(self) -> bool:
        """Establish persistent connection to C server"""
        try:
            with self.lock:
                # Close existing connection if any
                if self.socket:
                    try:
                        self.socket.close()
                    except:
                        pass
                
                # Create new socket
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.socket.settimeout(10.0)
                self.socket.connect((self.host, self.port))
                
                # Read welcome message
                welcome = self.socket.recv(1024).decode('utf-8').strip()
                print(f"Connected to C Server: {welcome}")
                
                self.connected = True
                return True
                
        except Exception as e:
            print(f"Failed to connect to C server at {self.host}:{self.port}")
            print(f"Error: {e}")
            self.socket = None
            self.connected = False
            return False

    def send_command(self, command: str) -> Optional[str]:
        """Send command to C server and get response"""
        # Reconnect if disconnected
        if not self.connected or not self.socket:
            print("[Bridge] Not connected, attempting to reconnect...")
            if not self._connect():
                return None

        try:
            with self.lock:
                # Send command with newline
                message = f"{command}\n"
                self.socket.sendall(message.encode('utf-8'))
                print(f"[DEBUG] Sent: {command}")

                # Receive response
                self.socket.settimeout(10.0)
                response = self.socket.recv(8192).decode('utf-8').strip()
                print(f"[DEBUG] Received: {response}")
                
                return response

        except (BrokenPipeError, ConnectionResetError, OSError) as e:
            print(f"[Bridge] Connection broken: {e}, reconnecting...")
            self.connected = False
            self.socket = None
            
            # Retry once
            if self._connect():
                try:
                    message = f"{command}\n"
                    self.socket.sendall(message.encode('utf-8'))
                    self.socket.settimeout(10.0)
                    response = self.socket.recv(8192).decode('utf-8').strip()
                    return response
                except:
                    return None
            return None
            
        except socket.timeout:
            print("[Bridge] Timeout waiting for response")
            return "ERROR|Timeout"
            
        except Exception as e:
            print(f"[Bridge] Error: {e}")
            self.connected = False
            self.socket = None
            return None

    def close(self):
        """Close connection"""
        with self.lock:
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
                self.socket = None
                self.connected = False
                print("[Bridge] Connection closed")


# Global singleton instance
_c_bridge_instance = None

def get_c_bridge() -> CSocketBridge:
    """Get singleton instance of CSocketBridge"""
    global _c_bridge_instance
    if _c_bridge_instance is None:
        _c_bridge_instance = CSocketBridge()
    return _c_bridge_instance
