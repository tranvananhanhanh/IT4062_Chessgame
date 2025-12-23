import socket
import threading
import time
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

    def _flush_buffer(self):
        """Flush any pending data in the socket buffer"""
        try:
            self.socket.setblocking(False)
            while True:
                try:
                    data = self.socket.recv(8192)
                    if not data:
                        break
                    print(f"[DEBUG] Flushed stale data: {data.decode('utf-8').strip()}")
                except BlockingIOError:
                    break
        except:
            pass
        finally:
            self.socket.setblocking(True)

    def _read_all_available(self) -> str:
        """Read all available data from socket with short timeout"""
        all_data = ""
        self.socket.settimeout(0.2)  # Short timeout
        try:
            while True:
                chunk = self.socket.recv(8192)
                if not chunk:
                    break
                all_data += chunk.decode('utf-8')
        except socket.timeout:
            pass
        except:
            pass
        return all_data

    def send_command(self, command: str) -> Optional[str]:
        """Send command to C server and get response"""
        # Reconnect if disconnected
        if not self.connected or not self.socket:
            print("[Bridge] Not connected, attempting to reconnect...")
            if not self._connect():
                return None

        try:
            with self.lock:
                # Flush any stale data before sending new command
                self._flush_buffer()
                
                # Send command with newline
                message = f"{command}\n"
                self.socket.sendall(message.encode('utf-8'))
                print(f"[DEBUG] Sent: {command}")

                # Wait a moment for server to process
                time.sleep(0.05)
                
                # Read initial response with longer timeout
                self.socket.settimeout(10.0)
                data = self.socket.recv(8192).decode('utf-8')
                
                # Brief pause then read any additional responses
                time.sleep(0.1)
                data += self._read_all_available()
                
                # Split by newline and filter empty lines
                responses = [r.strip() for r in data.split('\n') if r.strip()]
                print(f"[DEBUG] Received {len(responses)} responses: {responses}")
                
                if not responses:
                    return None
                
                # Determine expected response prefix based on command
                cmd_type = command.split('|')[0]
                expected_prefixes = {
                    'LOGIN': ['LOGIN_SUCCESS', 'LOGIN_FAILED', 'ERROR'],
                    'GET_STATS': ['STATS', 'ERROR'],
                    'GET_HISTORY': ['HISTORY', 'ERROR'],
                    'GET_REPLAY': ['REPLAY', 'ERROR'],
                    'GET_LEADERBOARD': ['LEADERBOARD', 'ERROR'],
                    'GET_ELO_HISTORY': ['ELO_HISTORY', 'ERROR'],
                    'START_MATCH': ['MATCH_CREATED', 'ERROR'],
                    'MOVE': ['MOVE_SUCCESS', 'MOVE_ERROR', 'CHECKMATE', 'STALEMATE', 'GAME_END', 'ERROR'],
                    'SURRENDER': ['SURRENDER_SUCCESS', 'ERROR'],
                }
                
                prefixes = expected_prefixes.get(cmd_type, [])
                
                # Find the best matching response
                response = None
                for r in responses:
                    for prefix in prefixes:
                        if r.startswith(prefix):
                            response = r
                            break
                    if response:
                        break
                
                # Fallback to last response if no match found
                if not response:
                    response = responses[-1]
                
                print(f"[DEBUG] Selected response: {response}")
                
                return response

        except (BrokenPipeError, ConnectionResetError, OSError) as e:
            print(f"[Bridge] Connection broken: {e}, reconnecting...")
            self.connected = False
            self.socket = None
            
            # Retry once
            if self._connect():
                return self.send_command(command)
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
