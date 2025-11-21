import socket
import os

# Cấu hình kết nối đến C Server
C_SERVER_HOST = os.environ.get('C_SERVER_HOST', '127.0.0.1')
C_SERVER_PORT = int(os.environ.get('C_SERVER_PORT', 12345))
BUFFER_SIZE = 4096

def send_to_c_server(command: str) -> str:
    """
    Kết nối đến C Server, gửi lệnh và nhận phản hồi.
    
    Args:
        command: Chuỗi lệnh theo giao thức định nghĩa (ví dụ: 'MOVE|...')
    
    Returns:
        Phản hồi từ C Server dưới dạng chuỗi (ví dụ: 'OK|...')
        
    Raises:
        socket.error: Nếu kết nối hoặc giao tiếp socket thất bại.
    """
    # Tạo một socket TCP
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    try:
        # Kết nối đến C Server
        client_socket.connect((C_SERVER_HOST, C_SERVER_PORT))
        print(f"[SocketBridge] Đã kết nối tới {C_SERVER_HOST}:{C_SERVER_PORT}")
        
        # Gửi lệnh. Thêm ký tự kết thúc ('\n') để C Server biết khi nào dừng đọc
        full_command = command + '\n'
        client_socket.sendall(full_command.encode('utf-8'))
        print(f"[SocketBridge] Đã gửi lệnh: {command}")
        
        # Nhận phản hồi từ C Server
        response_chunks = []
        while True:
            # Trong một giao thức hoàn chỉnh, bạn sẽ cần một cách xác định
            # điểm kết thúc của tin nhắn (ví dụ: một ký tự kết thúc đặc biệt)
            # Ở đây ta giả định C Server gửi phản hồi xong và đóng kết nối hoặc gửi một block đủ lớn.
            chunk = client_socket.recv(BUFFER_SIZE)
            if not chunk:
                break
            response_chunks.append(chunk)

        full_response = b"".join(response_chunks).decode('utf-8').strip()
        
        if not full_response:
             # Xử lý trường hợp server đóng kết nối mà không gửi dữ liệu
             return "ERROR|No response received from C Server"

        return full_response

    except ConnectionRefusedError:
        error_msg = f"Lỗi: C Server không hoạt động tại {C_SERVER_HOST}:{C_SERVER_PORT}. "
        error_msg += "Hãy đảm bảo c_server đang chạy!"
        raise ConnectionRefusedError(error_msg)
        
    except socket.timeout as e:
        raise socket.timeout(f"Hết thời gian chờ kết nối/nhận dữ liệu: {e}")
        
    except socket.error as e:
        raise socket.error(f"Lỗi socket: {e}")
        
    finally:
        # Luôn đóng socket
        client_socket.close()
        print("[SocketBridge] Đã đóng kết nối.")

# Ví dụ sử dụng (chỉ để kiểm tra)
if __name__ == '__main__':
    # Giả lập lệnh gửi (FEN: vị trí ban đầu)
    mock_command = "MOVE|game-123|HUMAN|rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1|e2e4"
    try:
        response = send_to_c_server(mock_command)
        print(f"\n[Test Result] Phản hồi cuối cùng: {response}")
    except Exception as e:
        print(f"\n[Test Result] Lỗi khi gửi lệnh: {e}")
