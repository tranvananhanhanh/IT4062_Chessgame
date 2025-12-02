# bridge/c_server_bridge.py - Low-level socket communication with C server
import socket
import threading
from typing import Optional


class CServerBridge:
    """Handles socket communication with C chess server"""
    
    def __init__(self, host='localhost', port=8888):
        self.host = host
        self.port = port
        self.sock: Optional[socket.socket] = None
        self.lock = threading.Lock()
        
    def connect(self) -> bool:
        """Establish connection to C server"""
        try:
            if self.sock:
                try:
                    self.sock.close()
                except:
                    pass
            
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(5.0)  # 5 second timeout
            self.sock.connect((self.host, self.port))
            print(f"[Bridge] Connected to C server at {self.host}:{self.port}")
            return True
        except socket.timeout:
            print(f"[Bridge] Connection timeout to C server at {self.host}:{self.port}")
            return False
        except Exception as e:
            print(f"[Bridge] Failed to connect to C server: {e}")
            return False
    
    def send_command(self, command: str) -> Optional[str]:
        """
        Send command to C server and get response
        
        Args:
            command: Command string to send (without newline)
            
        Returns:
            Response string from server, or None if error
        """
        with self.lock:
            try:
                if not self.sock:
                    if not self.connect():
                        return None
                
                # Send command
                print(f"[Bridge] Sent: {command.strip()}")
                self.sock.sendall((command + '\n').encode())
                
                # Receive response with timeout
                # ✅ FIX: Read until we get a complete line (ends with \n)
                try:
                    response = b""
                    while True:
                        chunk = self.sock.recv(4096)
                        if not chunk:
                            # Connection closed
                            print("[Bridge] Connection closed by server")
                            self.sock = None
                            return None
                        response += chunk
                        if b'\n' in response:
                            # Got complete response
                            break
                    
                    response_str = response.decode().strip()
                    print(f"[Bridge] Received: {response_str}")
                    return response_str
                except socket.timeout:
                    print("[Bridge] Timeout waiting for response from C server")
                    self.connect()  # Reconnect for next time
                    return None
                
            except (BrokenPipeError, ConnectionResetError) as e:
                print(f"[Bridge] Connection broken: {e}, reconnecting...")
                if self.connect():
                    # Retry once
                    try:
                        self.sock.sendall((command + '\n').encode())
                        response = self.sock.recv(4096).decode().strip()
                        return response
                    except:
                        return None
                return None
            except Exception as e:
                print(f"[Bridge] Error sending command: {e}")
                import traceback
                traceback.print_exc()
                return None
    
    def disconnect(self):
        """Close connection to C server"""
        if self.sock:
            try:
                self.sock.close()
            except:
                pass
            self.sock = None


# Singleton instance
_bridge_instance: Optional[CServerBridge] = None


def get_c_server_bridge() -> CServerBridge:
    """Get singleton instance of C server bridge"""
    global _bridge_instance
    if _bridge_instance is None:
        _bridge_instance = CServerBridge()
    return _bridge_instance
