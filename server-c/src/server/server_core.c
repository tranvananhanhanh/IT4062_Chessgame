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

    struct pollfd fds[MAX_CLIENTS + MAX_BOT_REQUESTS];
    ClientSession* sessions[MAX_CLIENTS] = {0};
    int nfds = 1;
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    memset(bot_requests, 0, sizeof(bot_requests)); // Clear pending bot requests

    time_t last_cleanup = time(NULL);
    time_t last_force_cleanup = time(NULL);

    while (1) {
        time_t now = time(NULL);
        if (now - last_cleanup >= 30) {
            game_manager_cleanup_finished_matches(&game_manager);
            last_cleanup = now;
        }
        if (now - last_force_cleanup >= 300) {
            game_manager_force_cleanup_stale_matches(&game_manager);
            last_force_cleanup = now;
        }

        // Build poll fds: clients & bot sockets
        nfds = 1;
        fds[0].fd = server_fd;
        fds[0].events = POLLIN;

        // Add client fds
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (sessions[i] && sessions[i]->socket_fd > 0) {
                fds[nfds].fd = sessions[i]->socket_fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }
        // Add bot fds
        for (int i = 0; i < MAX_BOT_REQUESTS; ++i) {
            if (bot_requests[i].active) {
                fds[nfds].fd = bot_requests[i].fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }

        int poll_count = poll(fds, nfds, 1000); // 1s timeout
        if (poll_count < 0) {
            perror("[Error] poll failed");
            break;
        }
        // Handle new client connection
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd >= 0 && nfds < MAX_CLIENTS + MAX_BOT_REQUESTS) {
                fds[nfds].fd = client_fd;
                fds[nfds].events = POLLIN;
                sessions[nfds] = client_session_create(client_fd);
                nfds++;
                printf("[Server] New connection from %s:%d (fd: %d)\n",
                       inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port),
                       client_fd);
                // Send welcome message
                const char *welcome = "WELCOME|Chess Server v1.0\n";
                send(client_fd, welcome, strlen(welcome), 0);
            }
        }
        // Scan all client and bot fds (skip server_fd at index 0)
        for (int i = 1; i < nfds; ++i) {
            if (fds[i].revents & POLLIN) {
                int fd = fds[i].fd;
                // Check if fd is in bot_requests
                int is_bot = 0;
                int bot_idx = -1;
                for (int b = 0; b < MAX_BOT_REQUESTS; ++b) {
                    if (bot_requests[b].active && bot_requests[b].fd == fd) {
                        is_bot = 1;
                        bot_idx = b;
                        break;
                    }
                }
                if (is_bot && bot_idx != -1) {
    char buffer[256];
    int n = recv(fd, buffer, sizeof(buffer)-1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        
        // Xóa ký tự xuống dòng nếu có
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        
        int game_id = bot_requests[bot_idx].game_id;
        printf("[BotAsync] Game %d bot trả: %s\n", game_id, buffer);

        // Tìm trận đấu
        GameMatch *match = game_manager_find_match(&game_manager, game_id);
        if (match) {
            pthread_mutex_lock(&match->lock);

            // Áp dụng nước đi bot lên board
            apply_bot_move_on_board(match, buffer, db_conn);

            // Cập nhật FEN sau khi bot đi
            chess_board_to_fen(&match->board, match->board. fen);

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

            // Tìm session của người chơi trắng để gửi
            if (match->white_player. socket_fd > 0) {
                send(match->white_player.socket_fd, response, strlen(response), 0);
                printf("[BotAsync] Sent to client (fd %d): %s", 
                    match->white_player.socket_fd, response);
            }
        }
    }

    // Dọn dẹp bot request
    close(fd);
    bot_requests[bot_idx].active = 0;
    bot_requests[bot_idx].fd = 0;
}
                // Regular client IO
                char buffer[BUFFER_SIZE] = {0};
                int bytes_read = recv(fd, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_read <= 0) {
                    // Client disconnected
                    for (int j = 0; j < MAX_CLIENTS; ++j) {
                        if (sessions[j] && sessions[j]->socket_fd == fd) {
                            client_session_handle_disconnect(sessions[j]);
                            close(fd);
                            client_session_destroy(sessions[j]);
                            sessions[j] = NULL;
                            break;
                        }
                    }
                    // Remove fd from poll list
                    for (int j = i; j < nfds - 1; ++j) {
                        fds[j] = fds[j+1];
                        sessions[j] = sessions[j+1];
                    }
                    nfds--;
                    i--;
                } else {
                    buffer[strcspn(buffer, "\n")] = 0;
                    printf("[Server] Received from client %d: %s\n", fd, buffer);
                    for (int j = 0; j < MAX_CLIENTS; ++j) {
                        if (sessions[j] && sessions[j]->socket_fd == fd) {
                            protocol_handle_command(sessions[j], buffer, db_conn);
                            break;
                        }
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
