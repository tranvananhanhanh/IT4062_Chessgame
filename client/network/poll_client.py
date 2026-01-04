import socket
import select
import sys

class PollClient:
    def __init__(self, host='127.0.0.1', port=8888):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        self.sock.setblocking(False)
        self.recv_buffer = b''
        self.send_queue = []

    def send(self, msg: str):
        self.send_queue.append(msg.encode() + b'\n')

    def poll(self, timeout=0):
        # Use select.select() for cross-platform compatibility (Windows + Linux)
        timeout_sec = timeout / 1000.0 if timeout > 0 else 0
        
        try:
            readable, writable, _ = select.select([self.sock], [self.sock], [], timeout_sec)
        except (OSError, ValueError):
            # Handle Windows compatibility issues
            readable, writable = [], []
        
        responses = []
        
        # Handle writable (send data)
        if self.sock in writable and self.send_queue:
            try:
                sent = self.sock.send(self.send_queue[0])
                self.send_queue[0] = self.send_queue[0][sent:]
                if not self.send_queue[0]:
                    self.send_queue.pop(0)
            except (BlockingIOError, BrokenPipeError):
                pass
        
        # Handle readable (receive data)
        if self.sock in readable:
            try:
                data = self.sock.recv(4096)
                if not data:
                    raise ConnectionError("Server closed")
                self.recv_buffer += data
            except (BlockingIOError, ConnectionResetError):
                pass
        
        # Parse messages from buffer
        while b'\n' in self.recv_buffer:
            line, self.recv_buffer = self.recv_buffer.split(b'\n', 1)
            msg = line.decode()
            print("[DEBUG PollClient] Message:", repr(msg))  # Debug line
            responses.append(msg)
        return responses
