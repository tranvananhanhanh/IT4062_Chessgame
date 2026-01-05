#include "server_core.h"
#include "client_session.h"
#include "protocol_handler.h"
#include "game.h"
#include "history.h"
#include "online_users.h"
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

/* ================= NON-BLOCKING BOT REQUEST MANAGEMENT ================= */
// Cấu trúc lưu trạng thái các request gửi sang Bot
typedef struct {
    int fd;          // Socket kết nối tới Bot
    int match_id;    // ID ván game đang chờ
    int user_id;     // User ID người chơi
    char difficulty[16]; // Difficulty level
    int active;      // 1 = đang chờ, 0 = rảnh
} BotRequest;

#define MAX_BOT_REQUESTS 100
BotRequest bot_requests[MAX_BOT_REQUESTS]; // Pool quản lý

/* ================= NON-BLOCKING BOT REQUEST HELPERS ================= */

/**
 * Register a bot request for non-blocking handling
 */
void register_bot_request(int match_id, int user_id, const char* fen, const char* difficulty) {
    // Gọi hàm gửi bên bot.c
    int bot_fd = send_request_to_bot_nonblocking(fen, difficulty);
    if (bot_fd < 0) {
        printf("[Error] Cannot connect to Bot service for match %d\n", match_id);
        return;
    }

    // Lưu vào danh sách để nhớ
    for (int i = 0; i < MAX_BOT_REQUESTS; i++) {
        if (!bot_requests[i].active) {
            bot_requests[i].fd = bot_fd;
            bot_requests[i].match_id = match_id;
            bot_requests[i].user_id = user_id;
            snprintf(bot_requests[i].difficulty, sizeof(bot_requests[i].difficulty), "%s", difficulty);
            bot_requests[i].active = 1;
            printf("[Async] Registered Bot request (fd: %d) for Match %d\n", bot_fd, match_id);
            return;
        }
    }
    printf("[Error] Too many bot requests pending! Closing fd %d\n", bot_fd);
    close(bot_fd);
}

/**
 * Process bot response when poll detects data
 */
void process_bot_response(int bot_fd, PGconn *db) {
    extern GameManager game_manager;
    
    // Tìm bot request tương ứng
    int match_id = -1;
    int user_id = -1;
    char difficulty[16] = "easy";
    
    for (int i = 0; i < MAX_BOT_REQUESTS; i++) {
        if (bot_requests[i].active && bot_requests[i].fd == bot_fd) {
            match_id = bot_requests[i].match_id;
            user_id = bot_requests[i].user_id;
            snprintf(difficulty, sizeof(difficulty), "%s", bot_requests[i].difficulty);
            
            // Dọn dẹp
            bot_requests[i].active = 0;
            bot_requests[i].fd = 0;
            break;
        }
    }
    
    if (match_id < 0) {
        printf("[Warn] Bot response for unknown match, closing fd %d\n", bot_fd);
        close(bot_fd);
        return;
    }
    
    // Đọc phản hồi từ bot
    char buffer[128];
    int n = recv(bot_fd, buffer, sizeof(buffer) - 1, 0);
    close(bot_fd); // Đóng socket ngay
    
    if (n <= 0) {
        printf("[Error] Bot did not respond for match %d\n", match_id);
        return;
    }
    
    buffer[n] = '\0';
    char *nl = strchr(buffer, '\n');
    if (nl) *nl = '\0';
    
    printf("[Async] Bot replied for Match %d: %s\n", match_id, buffer);
    
    // Xử lý bot move
    GameMatch *match = game_manager_find_match(&game_manager, match_id);
    if (!match) {
        printf("[Error] Match %d not found\n", match_id);
        return;
    }
    
    pthread_mutex_lock(&match->lock);
    
    if (strcmp(buffer, "NOMOVE") == 0 || strcmp(buffer, "ERROR") == 0) {
        // Bot không đi được → Draw hoặc lỗi
        match->result = RESULT_DRAW;
        match->status = GAME_FINISHED;
        pthread_mutex_unlock(&match->lock);
        return;
    }
    
    // Parse và execute bot move
    char bot_move[16];
    snprintf(bot_move, sizeof(bot_move), "%s", buffer);
    
    if (strlen(bot_move) < 4) {
        pthread_mutex_unlock(&match->lock);
        return;
    }
    
    char bfrom[3] = { bot_move[0], bot_move[1], '\0' };
    char bto[3]   = { bot_move[2], bot_move[3], '\0' };
    
    Move bm = {0};
    notation_to_coords(bfrom, &bm.from_row, &bm.from_col);
    notation_to_coords(bto, &bm.to_row, &bm.to_col);
    
    bm.piece = match->board.board[bm.from_row][bm.from_col];
    bm.captured_piece = match->board.board[bm.to_row][bm.to_col];
    
    // Check promotion
    bm.is_promotion = 0;
    bm.promotion_piece = '\0';
    if (strlen(bot_move) == 5) {
        char suffix = bot_move[4];
        if (suffix == 'q' || suffix == 'r' || suffix == 'b' || suffix == 'n') {
            bm.promotion_piece = (suffix == 'q') ? 'q' : (suffix == 'r') ? 'r' : 
                                 (suffix == 'b') ? 'b' : 'n';
            char piece_type = toupper(bm.piece);
            if (piece_type == 'P' && bm.to_row == 7) {
                bm.is_promotion = 1;
            }
        }
    }
    
    if (!validate_move(&match->board, &bm, COLOR_BLACK)) {
        printf("[ERROR] Invalid bot move rejected: %s\n", bot_move);
        match->status = GAME_FINISHED;
        match->result = RESULT_WHITE_WIN;
        pthread_mutex_unlock(&match->lock);
        return;
    }
    
    execute_move(&match->board, &bm);
    
    // Save bot move
    char fen_after_bot[FEN_MAX_LENGTH];
    chess_board_to_fen(&match->board, fen_after_bot);
    history_save_bot_move(db, match_id, bot_move, fen_after_bot);
    
    // Check end condition
    chess_board_to_fen(&match->board, match->board.fen);
    game_match_check_end_condition(match, db);
    
    // Send response to client
    char status[16] = "IN_GAME";
    if (match->status == GAME_FINISHED) {
        if (match->result == RESULT_WHITE_WIN) strcpy(status, "WHITE_WIN");
        else if (match->result == RESULT_BLACK_WIN) strcpy(status, "BLACK_WIN");
        else strcpy(status, "DRAW");
    }
    
    char resp[2048];
    snprintf(resp, sizeof(resp),
        "BOT_MOVE_RESULT|%s|%s|%s\n",
        match->board.fen, bot_move, status);
    
    send(match->white_player.socket_fd, resp, strlen(resp), 0);
    
    pthread_mutex_unlock(&match->lock);
}

/* ================= THREAD POOL (GIỮ LẠI CHO TƯƠNG LAI) ================= */
#include <pthread.h>
#include "bot.h" 

// 1. Cấu trúc dữ liệu cho hàng đợi
typedef struct {
    int game_id;
    char fen[512]; // Chuỗi FEN bàn cờ
    char difficulty[10]; // <--- THÊM MỚI
} BotTask;

typedef struct {
    int game_id;
    char move[16]; // Nước đi Bot trả về (VD: e2e4)
} BotResult;

// 2. Biến toàn cục quản lý Thread Pool
#define MAX_QUEUE 100
BotTask task_queue[MAX_QUEUE];
BotResult result_queue[MAX_QUEUE];
int task_count = 0;
int result_count = 0;

// Mutex để bảo vệ hàng đợi
pthread_mutex_t task_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t task_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;

// 3. Ống dẫn (Pipe) để đánh thức poll
// notify_pipe[0]: Đầu đọc (Main thread dùng)
// notify_pipe[1]: Đầu ghi (Worker thread dùng)
int notify_pipe[2];

// External global variables
extern GameManager game_manager;
extern PGconn *db_conn;
extern OnlineUsers online_users;

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


void* bot_worker_routine(void* arg) {
    while (1) {
        BotTask task;

        // 1. Lấy việc từ hàng đợi
        pthread_mutex_lock(&task_mutex);
        while (task_count == 0) {
            pthread_cond_wait(&task_cond, &task_mutex);
        }
        task = task_queue[--task_count];
        pthread_mutex_unlock(&task_mutex);

        // 2. GỌI HÀM THỰC TẾ CỦA BẠN (Blocking)
        char move_buffer[16] = {0};
        
        // Sử dụng call_python_bot thay vì bot_get_move
        // Hàm này sẽ kết nối socket tới Python và chờ phản hồi
        if (call_python_bot(task.fen, task.difficulty, move_buffer, sizeof(move_buffer)) != 0) {
            strcpy(move_buffer, "ERROR"); // Xử lý nếu gọi bot thất bại
        }

        // 3. Đẩy kết quả vào hàng đợi Result
        pthread_mutex_lock(&result_mutex);
        if (result_count < MAX_QUEUE) {
            result_queue[result_count].game_id = task.game_id;
            strcpy(result_queue[result_count].move, move_buffer);
            result_count++;
        }
        pthread_mutex_unlock(&result_mutex);

        // 4. Đánh thức Main Thread
        char wake_signal = '1';
        write(notify_pipe[1], &wake_signal, 1);
    }
    return NULL;
}

// Trong server_core.c (và nhớ sửa prototype trong header server_core.h)
void request_bot_move_async(int game_id, const char* fen, const char* difficulty) {
    pthread_mutex_lock(&task_mutex);
    if (task_count < MAX_QUEUE) {
        task_queue[task_count].game_id = game_id;
        strcpy(task_queue[task_count].fen, fen);
        strcpy(task_queue[task_count].difficulty, difficulty); // <--- Copy difficulty
        task_count++;
        pthread_cond_signal(&task_cond);
    } else {
        printf("[WARNING] Bot queue full!\n");
    }
    pthread_mutex_unlock(&task_mutex);
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
    
    
        // Tạo Pipe
    if (pipe(notify_pipe) < 0) {
        perror("Pipe creation failed");
        return -1;
    }
    
    // Tạo 2 Worker Threads chạy ngầm
    pthread_t tid;
    for(int i=0; i<2; i++) {
        pthread_create(&tid, NULL, bot_worker_routine, NULL);
        pthread_detach(tid); // Để thread tự hủy khi xong (dù nó chạy loop vĩnh viễn)
    }
    printf("[Server] Async Bot Workers initialized.\n");
    /* --------------------- */

    return 0;
}

// Start server and accept connections using poll for I/O multiplexing
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
    
    // Initialize bot_requests pool
    memset(bot_requests, 0, sizeof(bot_requests));
    
    // Setup FDS
    // Index 0: Server Socket (Lắng nghe kết nối)
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    // Index 1: Pipe (Lắng nghe tín hiệu từ Bot Thread - GIỮ LẠI CHO THREAD POOL)
    fds[1].fd = notify_pipe[0];
    fds[1].events = POLLIN;

    int nfds = 2; // Bắt đầu với 2 socket quan trọng

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
        
        // ===== ADD BOT SOCKETS TO POLL =====
        int bot_start_index = nfds; // Mark where bot sockets start
        for (int i = 0; i < MAX_BOT_REQUESTS; i++) {
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

        // --- SỬA ĐỔI 2: Xử lý tín hiệu từ Bot (Ưu tiên) ---
        if (fds[1].revents & POLLIN) {
            // 1. Đọc dữ liệu từ pipe để xóa sự kiện (Drain pipe)
            char wake_buff[16];
            read(fds[1].fd, wake_buff, sizeof(wake_buff));

            // 2. Lấy kết quả từ hàng đợi an toàn
            pthread_mutex_lock(&result_mutex);
            while (result_count > 0) {
                BotResult res = result_queue[--result_count];
                
                printf("[Async] Bot finished Game %d with move: %s\n", res.game_id, res.move);

                // 3. Cập nhật Logic Game (Code này chạy ở Main Thread nên an toàn)
                GameMatch* match = game_manager_find_match(&game_manager, res.game_id);
                if (match) {
                    // Logic xử lý nước đi của Bot (Tương tự protocol_handle_move)
                    // Gợi ý: Bạn nên viết hàm xử lý riêng, ví dụ: process_bot_move(match, res.move);
                    // Ở đây mình viết ví dụ logic gửi trả Client:
                    
                    char msg[64];
                    snprintf(msg, sizeof(msg), "OPPONENT_MOVE|%s\n", res.move);
                    
                    // Gửi cho người chơi (người chơi có thể là Trắng hoặc Đen)
                    if (!match->white_player.is_bot) {
                        send(match->white_player.socket_fd, msg, strlen(msg), 0);
                        send(match->white_player.socket_fd, "YOUR_TURN\n", 10, 0);
                    } 
                    else if (!match->black_player.is_bot) {
                        send(match->black_player.socket_fd, msg, strlen(msg), 0);
                        send(match->black_player.socket_fd, "YOUR_TURN\n", 10, 0);
                    }
                    
                    // Cập nhật bàn cờ trong memory của server...
                }
            }
            pthread_mutex_unlock(&result_mutex);
        }
        // ----------------------------------------------------

        // Handle New connection (Vẫn ở Index 0)
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd >= 0 && nfds < MAX_CLIENTS) {
                fds[nfds].fd = client_fd;
                fds[nfds].events = POLLIN;
                sessions[nfds] = client_session_create(client_fd);
                nfds++;
                printf("[Server] New connection from %s:%d (fd: %d)\n",
                       inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port),
                       client_fd);
                const char *welcome = "WELCOME|Chess Server v1.0\n";
                send(client_fd, welcome, strlen(welcome), 0);
            }
        }

        // Handle client IO and Bot sockets
        for (int i = 2; i < nfds; ++i) {
            if (fds[i].revents & POLLIN) {
                int fd = fds[i].fd;
                
                // ===== CHECK IF THIS IS A BOT SOCKET =====
                int is_bot_socket = 0;
                for (int b = 0; b < MAX_BOT_REQUESTS; b++) {
                    if (bot_requests[b].active && bot_requests[b].fd == fd) {
                        is_bot_socket = 1;
                        
                        // Process bot response
                        printf("[Non-blocking] Bot socket %d ready to read\n", fd);
                        process_bot_response(fd, db_conn);
                        
                        // Remove from poll list (already closed in process_bot_response)
                        for (int j = i; j < nfds - 1; ++j) {
                            fds[j] = fds[j+1];
                            sessions[j] = sessions[j+1];
                        }
                        nfds--;
                        i--;
                        break;
                    }
                }
                
                // ===== IF NOT BOT SOCKET, HANDLE AS CLIENT =====
                if (!is_bot_socket) {
                    char buffer[BUFFER_SIZE] = {0};
                    int bytes_read = recv(fd, buffer, BUFFER_SIZE - 1, 0);
                    if (bytes_read <= 0) {
                        // Client disconnected
                        client_session_handle_disconnect(sessions[i]);
                        close(fd);
                        client_session_destroy(sessions[i]);
                        // Remove from poll list
                        for (int j = i; j < nfds - 1; ++j) {
                            fds[j] = fds[j+1];
                            sessions[j] = sessions[j+1];
                        }
                        nfds--;
                        i--;
                    } else {
                        buffer[strcspn(buffer, "\n")] = 0;
                        printf("[Server] Received from client %d: %s\n", fd, buffer);
                        protocol_handle_command(sessions[i], buffer, db_conn);
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
