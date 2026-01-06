#include "server_core.h"
#include "client_session.h"
#include "protocol_handler.h"
#include "game.h"
#include "history.h"
#include "online_users.h"
#include "bot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#define MAX_CLIENTS 1024

#define MAX_BOT_REQUESTS 100
typedef struct {
    int fd;         // socket tới bot server
    int game_id;    // id ván chơi cần bot trả về
    int active;     // cờ hiệu có đang sử dụng
} BotRequest;

BotRequest bot_requests[MAX_BOT_REQUESTS];

// External global variables
extern GameManager game_manager;
extern PGconn *db_conn;
extern OnlineUsers online_users;

void register_bot_request(int game_id, const char* fen, const char* difficulty) {
    int bot_fd = send_request_to_bot_nonblocking(fen, difficulty);
    if (bot_fd < 0) {
        printf("[Error] Cannot connect to Bot!\n");
        return;
    }
    for (int i = 0; i < MAX_BOT_REQUESTS; i++) {
        if (!bot_requests[i].active) {
            bot_requests[i].fd = bot_fd;
            bot_requests[i].game_id = game_id;
            bot_requests[i].active = 1;
            printf("[Async] Register bot_req (fd=%d) for Game %d\n", bot_fd, game_id);
            return;
        }
    }
    printf("[Error] Out of bot request slots!\n");
    close(bot_fd);
}


// Client handler thread
void* client_handler(void *arg) {
    ClientSession *session = (ClientSession*)arg;
    int client_fd = session->socket_fd;
    char buffer[BUFFER_SIZE];
    
    printf("[Server] Client %d connected\n", client_fd);
    
    // Send welcome message
    const char *welcome = "WELCOME|Chess Server v1.0\n";
    send(client_fd, welcome, strlen(welcome), 0);
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_read <= 0) {
            // Client disconnected
            client_session_handle_disconnect(session);
            break;
        }
        
        // Remove newline
        buffer[strcspn(buffer, "\n")] = 0;
        
        printf("[Server] Received from client %d: %s\n", client_fd, buffer);
        
        // Handle command
        protocol_handle_command(session, buffer, db_conn);
    }
    
    close(client_fd);
    client_session_destroy(session);
    return NULL;
}

// Server initialization
int server_init(PGconn **db_connection) {
    printf("========================================\n");
    printf("     Chess Game Server Starting...     \n");
    printf("========================================\n\n");
    
    // Connect to database
    *db_connection = db_connect();
    if (*db_connection == NULL) {
        fprintf(stderr, "[Error] Failed to connect to database\n");
        return -1;
    }
    
    // Initialize game manager
    game_manager_init(&game_manager);
    printf("[Server] Game manager initialized\n");

    pthread_mutex_init(&online_users.lock, NULL);
    online_users.count = 0;
    
    return 0;
}

// Start server and accept connections using poll for I/O multiplexing,
// now updated to also manage Bot sockets (non-blocking event driven)
// Includes bot_requests pool and handles both client and bot responses in one event loop

void server_start() {
    // 1. Tạo và cấu hình Socket Server
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[Error] Socket creation failed");
        return;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[Error] Bind failed");
        close(server_fd);
        return;
    }
    
    if (listen(server_fd, 10) < 0) {
        perror("[Error] Listen failed");
        close(server_fd);
        return;
    }
    
    printf("[Server] Listening on port %d...\n", PORT);
    printf("[Server] Ready to accept connections!\n\n");

    // 2. Khởi tạo cấu trúc quản lý
    struct pollfd fds[MAX_CLIENTS + MAX_BOT_REQUESTS + 1]; // +1 cho server_fd
    ClientSession* sessions[MAX_CLIENTS] = {0}; // Mảng quản lý session client
    
    // Xóa bot requests cũ
    memset(bot_requests, 0, sizeof(bot_requests));

    time_t last_cleanup = time(NULL);
    time_t last_force_cleanup = time(NULL);

    // 3. Vòng lặp chính
    while (1) {
        // --- A. Xử lý dọn dẹp định kỳ ---
        time_t now = time(NULL);
        if (now - last_cleanup >= 30) {
            game_manager_cleanup_finished_matches(&game_manager);
            last_cleanup = now;
        }
        if (now - last_force_cleanup >= 300) {
            game_manager_force_cleanup_stale_matches(&game_manager);
            last_force_cleanup = now;
        }

        // --- B. Xây dựng lại danh sách pollfd (Quan trọng) ---
        // Luôn reset danh sách theo dõi để tránh lỗi chỉ số khi client out
        int nfds = 0;

        // 1. Thêm Server Socket
        fds[nfds].fd = server_fd;
        fds[nfds].events = POLLIN;
        nfds++;

        // 2. Thêm Client Sockets đang hoạt động
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (sessions[i] && sessions[i]->socket_fd > 0) {
                fds[nfds].fd = sessions[i]->socket_fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }

        // 3. Thêm Bot Sockets đang active
        for (int i = 0; i < MAX_BOT_REQUESTS; ++i) {
            if (bot_requests[i].active && bot_requests[i].fd > 0) {
                fds[nfds].fd = bot_requests[i].fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }

        // --- C. Chờ sự kiện (Poll) ---
        int poll_count = poll(fds, nfds, 1000); // Timeout 1s
        if (poll_count < 0) {
            perror("[Error] poll failed");
            break;
        }
        if (poll_count == 0) continue; // Timeout, quay lại vòng lặp

        // --- D. Xử lý kết nối mới (Server Socket) ---
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd >= 0) {
                // Tìm slot trống trong mảng sessions
                int found_slot = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (sessions[i] == NULL) {
                        sessions[i] = client_session_create(client_fd);
                        printf("[Server] New connection from %s:%d (fd: %d)\n",
                               inet_ntoa(client_addr.sin_addr),
                               ntohs(client_addr.sin_port),
                               client_fd);
                        
                        const char *welcome = "WELCOME|Chess Server v1.0\n";
                        send(client_fd, welcome, strlen(welcome), 0);
                        found_slot = 1;
                        break;
                    }
                }
                if (!found_slot) {
                    printf("[Server] Server full, rejected client fd %d\n", client_fd);
                    close(client_fd);
                }
            }
        }

        // --- E. Xử lý dữ liệu từ Client và Bot ---
        // Duyệt từ 1 vì 0 là server_fd
        for (int i = 1; i < nfds; ++i) {
            if (fds[i].revents & POLLIN) {
                int fd = fds[i].fd;
                
                // --- Kiểm tra xem FD này là BOT hay CLIENT ---
                int is_bot = 0;
                int bot_idx = -1;
                
                // Check danh sách Bot
                for (int b = 0; b < MAX_BOT_REQUESTS; ++b) {
                    if (bot_requests[b].active && bot_requests[b].fd == fd) {
                        is_bot = 1;
                        bot_idx = b;
                        break;
                    }
                }

                if (is_bot && bot_idx != -1) {
                    // ============ XỬ LÝ BOT (Theo code bạn gửi) ============
                    char buffer[256];
                    int n = recv(fd, buffer, sizeof(buffer)-1, 0);
                    if (n > 0) {
                        buffer[n] = '\0';
                        
                        // Xóa ký tự xuống dòng
                        char *newline = strchr(buffer, '\n');
                        if (newline) *newline = '\0';
                        
                        int game_id = bot_requests[bot_idx].game_id;
                        printf("[BotAsync] Game %d bot trả: %s\n", game_id, buffer);

                        // Tìm trận đấu
                        GameMatch *match = game_manager_find_match(&game_manager, game_id);
                        if (match) {
                            pthread_mutex_lock(&match->lock);

                            // Áp dụng nước đi bot
                            apply_bot_move_on_board(match, buffer, db_conn);

                            // Cập nhật FEN
                            chess_board_to_fen(&match->board, match->board.fen);

                            // Kiểm tra kết thúc trận
                            game_match_check_end_condition(match, db_conn);

                            // Xác định trạng thái
                            char status[16] = "IN_GAME";
                            if (match->status == GAME_FINISHED) {
                                if (match->result == RESULT_WHITE_WIN) strcpy(status, "WHITE_WIN");
                                else if (match->result == RESULT_BLACK_WIN) strcpy(status, "BLACK_WIN");
                                else strcpy(status, "DRAW");
                            }

                            pthread_mutex_unlock(&match->lock);

                            // Gửi kết quả về client (người chơi trắng)
                            char response[1024];
                            snprintf(response, sizeof(response),
                                "BOT_MOVE_RESULT|%s|%s|%s\n",
                                match->board.fen, buffer, status);

                            if (match->white_player.socket_fd > 0) {
                                send(match->white_player.socket_fd, response, strlen(response), 0);
                                printf("[BotAsync] Sent to client (fd %d): %s", 
                                    match->white_player.socket_fd, response);
                            }
                        }
                    }
                    // Dọn dẹp bot request sau khi nhận xong
                    close(fd);
                    bot_requests[bot_idx].active = 0;
                    bot_requests[bot_idx].fd = 0;

                } else {
                    // ============ XỬ LÝ CLIENT THƯỜNG ============
                    ClientSession *current_session = NULL;
                    int session_idx = -1;

                    // Tìm session tương ứng với fd
                    for (int s = 0; s < MAX_CLIENTS; s++) {
                        if (sessions[s] && sessions[s]->socket_fd == fd) {
                            current_session = sessions[s];
                            session_idx = s;
                            break;
                        }
                    }

                    if (current_session) {
                        char buffer[BUFFER_SIZE] = {0};
                        int bytes_read = recv(fd, buffer, BUFFER_SIZE - 1, 0);
                        
                        if (bytes_read <= 0) {
                            // Client ngắt kết nối
                            printf("[Server] Client disconnected (fd: %d)\n", fd);
                            client_session_handle_disconnect(current_session);
                            close(fd);
                            client_session_destroy(current_session);
                            sessions[session_idx] = NULL; // Đánh dấu slot trống
                        } else {
                            // Xử lý lệnh
                            buffer[strcspn(buffer, "\n")] = 0;
                            printf("[Server] Received from client %d: %s\n", fd, buffer);
                            protocol_handle_command(current_session, buffer, db_conn);
                        }
                    } else {
                        // FD lạ (không phải bot, không phải session hợp lệ), đóng để an toàn
                        close(fd);
                    }
                }
            }
        }
    }
    close(server_fd);
}
// Shutdown server
void server_shutdown() {
    printf("\n[Server] Shutting down...\n");
    
    if (db_conn != NULL) {
        db_disconnect(db_conn);
        printf("[Server] Database connection closed\n");
    }
    
    printf("[Server] Shutdown complete\n");
}
