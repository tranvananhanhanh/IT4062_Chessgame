import socket
import os

# Cấu hình kết nối đến C Server
C_SERVER_HOST = os.environ.get('C_SERVER_HOST', '127.0.0.1')
C_SERVER_PORT = int(os.environ.get('C_SERVER_PORT', 12345))
BUFFER_SIZE = 4096

def send_to_c_server(command: str) -> str:
    """
    Kết nối đến C Server, gửi lệnh và nhận phản hồi.
    Đây là cầu nối TCP Client giữa Flask và C Server.
    
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
        # Thiết lập timeout cho kết nối và nhận dữ liệu
        client_socket.settimeout(5.0) 
        
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
            chunk = client_socket.recv(BUFFER_SIZE)
            if not chunk:
                break
            response_chunks.append(chunk)
            if b'\n' in chunk:
                break

        full_response = b"".join(response_chunks).decode('utf-8').strip()
        if not full_response:
            return "ERROR|No response received from C Server"
        # Nếu server trả về nhiều dòng, lấy dòng đầu tiên
        full_response = full_response.splitlines()[0]
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
