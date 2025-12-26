import socket
import select

class PollClient:
    def __init__(self, host='127.0.0.1', port=8888):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        self.sock.setblocking(False)
        self.poller = select.poll()
        self.poller.register(self.sock, select.POLLIN | select.POLLOUT)
        self.recv_buffer = b''
        self.send_queue = []

    def send(self, msg: str):
        self.send_queue.append(msg.encode() + b'\n')

    def poll(self, timeout=0):
        events = self.poller.poll(timeout)
        responses = []
        for fd, event in events:
            if event & select.POLLOUT and self.send_queue:
                try:
                    sent = self.sock.send(self.send_queue[0])
                    self.send_queue[0] = self.send_queue[0][sent:]
                    if not self.send_queue[0]:
                        self.send_queue.pop(0)
                except BlockingIOError:
                    pass
            if event & select.POLLIN:
                try:
                    data = self.sock.recv(4096)
                    if not data:
                        raise ConnectionError("Server closed")
                    self.recv_buffer += data
                except BlockingIOError:
                    pass
        while b'\n' in self.recv_buffer:
            line, self.recv_buffer = self.recv_buffer.split(b'\n', 1)
            msg = line.decode()
            print("[DEBUG PollClient] Message:", repr(msg))  # Debug line
            responses.append(msg)
        return responses
