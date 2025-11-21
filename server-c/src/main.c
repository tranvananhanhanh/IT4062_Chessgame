// MÔ PHỎNG: C Server - main.c
// File này chỉ nhằm mục đích minh họa kiến trúc TCP Listener cơ bản.
// Để có một Chess AI Engine hoàn chỉnh, bạn cần thư viện socket và logic phức tạp hơn.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <ctype.h>
#include "include/protocol_common.h"

#define PORT 12345
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 5

void handle_client(int client_socket);
char* process_chess_move(const char* command);

// Forward declarations from protocol_handler
char* dispatch_command_string(const char *command);
void init_global_session(void);

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int opt = 1; // single definition

    // 1. Tạo file descriptor cho socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Gắn socket vào cổng 12345 (Quan trọng: Phải tắt TIME_WAIT nếu cần)
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
    }
#ifdef SO_REUSEPORT
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEPORT failed (continuing)");
    }
#endif

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Lắng nghe trên mọi interface
    address.sin_port = htons(PORT);

    // 2. Gắn socket vào cổng
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // 3. Lắng nghe kết nối đến (tối đa MAX_CLIENTS trong hàng đợi)
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("--- C Chess AI Server ---\n");
    printf("Server đang lắng nghe tại cổng %d...\n", PORT);

    // Initialize global session
    init_global_session();

    while (1) {
        printf("\nChờ kết nối mới...\n");

        // 4. Chấp nhận kết nối
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        // 5. Xử lý kết nối trong một hàm riêng biệt
        handle_client(new_socket);

        // Đóng socket sau khi xử lý xong (Flask gửi xong sẽ đóng)
        close(new_socket);
    }

    return 0;
}

// Xử lý từng kết nối client
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t valread;
    char* response;

    // Đọc dữ liệu từ client (Flask)
    valread = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (valread > 0) {
        buffer[valread] = '\0';
        printf("[Client] Đã nhận lệnh: %s", buffer);

        // Gọi hàm xử lý logic game (giả lập)
        response = process_chess_move(buffer);

        // Gửi phản hồi lại cho client (Flask)
        send(client_socket, response, strlen(response), 0);
        printf("[Server] Đã gửi phản hồi: %s\n", response);
        
        free(response); // Giải phóng bộ nhớ chuỗi phản hồi
    } else if (valread == 0) {
        printf("[Client] Ngắt kết nối\n");
    } else {
        perror("Read error");
    }
}

// Hàm xử lý logic cờ vua
char* process_chess_move(const char* command) {
    // Delegate to protocol dispatcher instead of using mock logic here
    return dispatch_command_string(command);
}
