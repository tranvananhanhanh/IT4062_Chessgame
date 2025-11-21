#ifndef PROTOCOL_COMMON_H
#define PROTOCOL_COMMON_H

#include <stdbool.h>
#include <string.h>

// ===============================================
// 1. ĐỊNH NGHĨA CẤU TRÚC DỮ LIỆU VÀ GIAO THỨC
// ===============================================

// Định nghĩa Trạng thái (State) của Trận đấu
typedef enum {
    STATE_TERMINATED = 0,
    STATE_WAIT_MODE = 1,      // Chờ chọn chế độ (Bot)
    STATE_WAIT_MATCH = 2,     // Chờ ghép cặp (1v1)
    STATE_IN_GAME = 3,        // Đang diễn ra
    STATE_PAUSED = 4,         // Tạm dừng
    STATE_WAIT_DRAW_RESP = 5, // Chờ phản hồi hòa
    STATE_WAIT_REMATCH_RESP = 6, // Chờ phản hồi đấu lại
    STATE_GAME_OVER = 7       // Kết thúc
} GameState;

// Định nghĩa Lệnh (Command)
typedef enum {
    CMD_UNKNOWN = 0,
    // UC001 - Match Control
    CMD_PAUSE, CMD_RESUME, CMD_SURRENDER, CMD_DRAW, 
    CMD_DRAW_ACCEPT, CMD_DRAW_DECLINE, CMD_REMATCH, 
    CMD_REMATCH_ACCEPT, CMD_REMATCH_DECLINE,
    // UC002/UC003 - Game Play
    CMD_MODE_BOT, CMD_MOVE, CMD_QUIT,
    // UC003 - 1v1 Specific
    CMD_HELLO, CMD_MATCH_FOUND 
} CommandType;

// Cấu trúc đại diện cho một phiên Game (Game Session)
typedef struct {
    int match_id;
    int player_id;
    int opponent_id; // Bot ID hoặc Player ID
    bool is_bot_match;
    GameState current_state;
    // Dữ liệu khác cần thiết cho logic
    char current_fen[256];
} GameSession;

// ===============================================
// 2. MÔ PHỎNG CÁC HÀM CẤP THẤP (Mocked Low-Level Functions)
// ===============================================

// Gửi tin nhắn TCP đến một Client
void send_to_client(int client_fd, const char *message);

// Cập nhật cơ sở dữ liệu
void update_database(const char *query);

// Gọi AI Bot để lấy nước đi
char* call_ai_bot(const char *fen, char *ai_move_buffer, size_t buffer_size);

// Kiểm tra tính hợp lệ của nước đi
bool validate_move(const char *fen, const char *move);

// Kiểm tra kết thúc trận đấu (Checkmate, Stalemate)
bool check_game_end(const char *new_fen, char *result_status, size_t status_size);

#endif // PROTOCOL_COMMON_H
