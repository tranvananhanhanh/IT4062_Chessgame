// Bộ Điều phối Giao thức Chính 
// Nhận lệnh thô, phân tích và gọi đến module UC tương ứng.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>            // for close()
#include <sys/types.h>         // for socket types
#include <sys/socket.h>        // for socket(), connect(), send(), recv(), setsockopt()
#include <netinet/in.h>        // for sockaddr_in, htons
#include <arpa/inet.h>         // for inet_pton
#include <sys/time.h>          // for struct timeval
#include "include/protocol_common.h"

// Định nghĩa kích thước buffer cục bộ cho file này
#define PH_BUFFER_SIZE 4096

// Forward declarations for handlers implemented in other translation units
void handle_match_control(GameSession *session, CommandType cmd, int client_fd, int opponent_fd);
void handle_bot_game(GameSession *session, CommandType cmd, const char *params, int client_fd);

// ===============================================
// 2. MÔ PHỎNG CÁC HÀM CẤP THẤP (TRIỂN KHAI)
// ===============================================
// Các hàm này được khai báo trong protocol_common.h và được gọi bởi các module UC

void send_to_client(int client_fd, const char *message) {
    printf("[SIM] -> Client %d: %s\n", client_fd, message);
    // Trong thực tế: send(client_fd, message, strlen(message), 0);
}

// DB operations are now handled entirely by the Python backend.
// This function is kept only to satisfy linker references but does nothing.
void update_database(const char *query) {
    (void)query; // suppress unused parameter warning
}

/**
 * Gọi Python bot:
 * input:  current FEN (trước nước người)
 * output:
 *   - ai_move_buffer: nước đi của bot (VD: "e7e5")
 *   - return value:  một chuỗi dạng "fen_after_player|fen_after_bot"
 * Caller phải free() chuỗi trả về.
 */
char* call_ai_bot(const char *fen, char *ai_move_buffer, size_t buffer_size) {
    if (!fen || !ai_move_buffer || buffer_size == 0) {
        return NULL;
    }

    const char *host = getenv("BOT_HOST") ? getenv("BOT_HOST") : "127.0.0.1";
    int port = getenv("BOT_PORT") ? atoi(getenv("BOT_PORT")) : 5001;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[AI_SOCKET] socket");
        strncpy(ai_move_buffer, "NONE", buffer_size);
        ai_move_buffer[buffer_size - 1] = '\0';
        return NULL;
    }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port   = htons(port);

    if (inet_pton(AF_INET, host, &serv.sin_addr) <= 0) {
        perror("[AI_SOCKET] inet_pton");
        close(sock);
        strncpy(ai_move_buffer, "NONE", buffer_size);
        ai_move_buffer[buffer_size - 1] = '\0';
        return NULL;
    }

    struct timeval tv;
    tv.tv_sec  = 3;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

    if (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("[AI_SOCKET] connect");
        close(sock);
        strncpy(ai_move_buffer, "NONE", buffer_size);
        ai_move_buffer[buffer_size - 1] = '\0';
        return NULL;
    }

    // gửi FEN + '\n'
    char outbuf[4096];
    snprintf(outbuf, sizeof(outbuf), "%s\n", fen);
    ssize_t sent = send(sock, outbuf, strlen(outbuf), 0);
    if (sent <= 0) {
        perror("[AI_SOCKET] send");
        close(sock);
        strncpy(ai_move_buffer, "NONE", buffer_size);
        ai_move_buffer[buffer_size - 1] = '\0';
        return NULL;
    }

    // nhận "fen_after_player|bot_move|fen_after_bot\n"
    char inbuf[8192];
    ssize_t recvd = 0;
    size_t pos = 0;
    while ((recvd = recv(sock, inbuf + pos,
                         sizeof(inbuf) - pos - 1, 0)) > 0) {
        pos += recvd;
        if (inbuf[pos - 1] == '\n' || pos >= sizeof(inbuf) - 2) {
            break;
        }
    }
    close(sock);

    if (pos <= 0) {
        fprintf(stderr, "[AI_SOCKET] empty response\n");
        strncpy(ai_move_buffer, "NONE", buffer_size);
        ai_move_buffer[buffer_size - 1] = '\0';
        return NULL;
    }

    inbuf[pos] = '\0';

    // parse "fen_after_player|bot_move|fen_after_bot"
    char *p1 = strchr(inbuf, '|');
    if (!p1) {
        fprintf(stderr, "[AI_SOCKET] invalid response format: %s\n", inbuf);
        strncpy(ai_move_buffer, "NONE", buffer_size);
        ai_move_buffer[buffer_size - 1] = '\0';
        return NULL;
    }
    *p1 = '\0';
    char *fen_after_player = inbuf;

    char *p2 = strchr(p1 + 1, '|');
    if (!p2) {
        fprintf(stderr, "[AI_SOCKET] invalid response format (missing second '|'): %s\n", inbuf);
        strncpy(ai_move_buffer, "NONE", buffer_size);
        ai_move_buffer[buffer_size - 1] = '\0';
        return NULL;
    }
    *p2 = '\0';
    char *bot_move = p1 + 1;
    char *fen_after_bot = p2 + 1;

    // trim newline
    char *nl = strchr(fen_after_bot, '\n');
    if (nl) *nl = '\0';

    // copy bot_move ra buffer
    strncpy(ai_move_buffer, bot_move, buffer_size);
    ai_move_buffer[buffer_size - 1] = '\0';

    if (strcmp(bot_move, "NONE") == 0) {
        return NULL;
    }

    // Trả lại "fen_after_player|fen_after_bot"
    size_t len1 = strlen(fen_after_player);
    size_t len2 = strlen(fen_after_bot);
    size_t total_len = len1 + 1 + len2 + 1;
    char *result = malloc(total_len);
    if (!result) {
        fprintf(stderr, "[AI_SOCKET] malloc failed\n");
        return NULL;
    }
    snprintf(result, total_len, "%s|%s", fen_after_player, fen_after_bot);
    return result;
}

bool validate_move(const char *fen, const char *move) {
    printf("[LOGIC] Validating move %s on FEN %s...\n", move, fen);
    return true; // Mock: luôn hợp lệ
}

bool check_game_end(const char *new_fen, char *result_status, size_t status_size) {
    if (strstr(new_fen, "K3k3") != NULL) {
        strncpy(result_status, "CHECKMATE", status_size);
        return true;
    }
    strncpy(result_status, "IN_GAME", status_size);
    return false;
}

// === Simple global session and dispatcher for main.c ===
static GameSession g_session;

void init_global_session(void) {
    memset(&g_session, 0, sizeof(g_session));
    g_session.current_state = STATE_WAIT_MODE;
}

// Bridge: take raw command from main.c and return protocol response string (malloc'ed)
char* dispatch_command_string(const char *command) {
    // MODE BOT
    if (strncmp(command, "MODE BOT", 8) == 0) {
        const char* start_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        memset(&g_session, 0, sizeof(g_session));
        g_session.is_bot_match  = true;
        g_session.current_state = STATE_IN_GAME;
        strncpy(g_session.current_fen, start_fen, sizeof(g_session.current_fen));
        g_session.current_fen[sizeof(g_session.current_fen) - 1] = '\0';

        char *resp = (char*)malloc(512);
        snprintf(resp, 512, "OK|%s|NONE|MODE BOT READY", start_fen);
        return resp;
    }

    // MOVE|gameId|fen|uci
    if (strncmp(command, "MOVE|", 5) == 0) {
        char buf[PH_BUFFER_SIZE];
        strncpy(buf, command, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        char *saveptr = NULL;
        strtok_r(buf, "|", &saveptr);              // "MOVE"
        char *game_id = strtok_r(NULL, "|", &saveptr); // unused currently
        char *fen     = strtok_r(NULL, "|", &saveptr);
        char *uci     = strtok_r(NULL, "|", &saveptr);

        if (!fen || !uci) {
            char *err = (char*)malloc(64);
            strcpy(err, "ERROR|BAD_MOVE_FORMAT");
            return err;
        }

        // Sync session with FEN from Flask
        strncpy(g_session.current_fen, fen, sizeof(g_session.current_fen));
        g_session.current_fen[sizeof(g_session.current_fen) - 1] = '\0';
        g_session.current_state = STATE_IN_GAME;
        g_session.is_bot_match  = true;

        if (!validate_move(g_session.current_fen, uci)) {
            char *err = (char*)malloc(64);
            strcpy(err, "ERROR|INVALID_MOVE");
            return err;
        }

        // Call Python AI: expect "fen_after_player|bot_move|fen_after_bot"
        char bot_move[10] = {0};
        char *fen_combo = call_ai_bot(g_session.current_fen, bot_move, sizeof(bot_move));
        if (!fen_combo) {
            char *err = (char*)malloc(64);
            strcpy(err, "ERROR|AI_FAILED");
            return err;
        }

        char *sep1 = strchr(fen_combo, '|');
        if (!sep1) {
            free(fen_combo);
            char *err = (char*)malloc(64);
            strcpy(err, "ERROR|AI_BAD_FORMAT");
            return err;
        }
        *sep1 = '\0';
        const char *fen_after_player = fen_combo;
        const char *fen_after_bot    = sep1 + 1;

        // Update session FEN to bot FEN
        strncpy(g_session.current_fen, fen_after_bot, sizeof(g_session.current_fen));
        g_session.current_fen[sizeof(g_session.current_fen) - 1] = '\0';

        char status_msg[32] = "IN_GAME";
        if (check_game_end(g_session.current_fen, status_msg, sizeof(status_msg))) {
            g_session.current_state = STATE_GAME_OVER;
        }

        char *resp = (char*)malloc(1024);
        snprintf(resp, 1024, "OK|%s|%s|%s|%s",
                 fen_after_player,
                 bot_move[0] ? bot_move : "NONE",
                 fen_after_bot,
                 status_msg);
        free(fen_combo);
        return resp;
    }

    // CONTROL|gameId|userId|ACTION
    if (strncmp(command, "CONTROL|", 8) == 0) {
        // Parse CONTROL header
        char buf[PH_BUFFER_SIZE];
        strncpy(buf, command, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        char *saveptr = NULL;
        strtok_r(buf, "|", &saveptr);              // "CONTROL"
        char *game_id_str = strtok_r(NULL, "|", &saveptr); // game_id (chưa dùng)
        char *user_id_str = strtok_r(NULL, "|", &saveptr); // user_id (chưa dùng)
        char *action_str  = strtok_r(NULL, "|", &saveptr); // PAUSE / RESUME / ...

        if (!action_str) {
            char *err = (char*)malloc(64);
            strcpy(err, "ERROR|BAD_CONTROL_FORMAT");
            return err;
        }

        // Trim trailing CR/LF and spaces from action_str to avoid UNKNOWN_ACTION
        size_t alen = strlen(action_str);
        while (alen > 0 && (action_str[alen - 1] == '\n' || action_str[alen - 1] == '\r' || action_str[alen - 1] == ' ' || action_str[alen - 1] == '\t')) {
            action_str[alen - 1] = '\0';
            alen--;
        }

        // Map ACTION string -> CommandType
        CommandType cmd = CMD_UNKNOWN;
        if (strcmp(action_str, "PAUSE") == 0) cmd = CMD_PAUSE;
        else if (strcmp(action_str, "RESUME") == 0) cmd = CMD_RESUME;
        else if (strcmp(action_str, "SURRENDER") == 0) cmd = CMD_SURRENDER;
        else if (strcmp(action_str, "DRAW") == 0) cmd = CMD_DRAW;
        else if (strcmp(action_str, "DRAW_ACCEPT") == 0) cmd = CMD_DRAW_ACCEPT;
        else if (strcmp(action_str, "DRAW_DECLINE") == 0) cmd = CMD_DRAW_DECLINE;
        else if (strcmp(action_str, "REMATCH") == 0) cmd = CMD_REMATCH;
        else if (strcmp(action_str, "REMATCH_ACCEPT") == 0) cmd = CMD_REMATCH_ACCEPT;
        else if (strcmp(action_str, "REMATCH_DECLINE") == 0) cmd = CMD_REMATCH_DECLINE;

        if (cmd == CMD_UNKNOWN) {
            char *err = (char*)malloc(64);
            strcpy(err, "ERROR|UNKNOWN_ACTION");
            return err;
        }

        // Tùy vào loại session, route sang UC001 hoặc UC002
        // Ở đây ta coi bot match cũng dùng chung pause/surrender/draw.
        // client_fd / opponent_fd là mock (0,1) vì Flask không dùng socket này.
        int client_fd   = 0;
        int opponent_fd = 1;

        if (g_session.is_bot_match) {
            // Với bot match: reuse handle_match_control cho các CONTROL (ngoại trừ MOVE/MODE)
            handle_match_control(&g_session, cmd, client_fd, opponent_fd);
        } else {
            handle_match_control(&g_session, cmd, client_fd, opponent_fd);
        }

        // Vì send_to_client chỉ in ra log, ta không đọc được message chi tiết.
        // Để đơn giản cho Flask, ta trả về OK|<ACTION> hoặc OK|GAME_OVER...
        char *resp = (char*)malloc(128);
        if (cmd == CMD_PAUSE) {
            strcpy(resp, "OK|GAME_PAUSED");
        } else if (cmd == CMD_RESUME) {
            strcpy(resp, "OK|GAME_RESUMED");
        } else if (cmd == CMD_SURRENDER) {
            strcpy(resp, "OK|SURRENDERED");
        } else if (cmd == CMD_DRAW) {
            strcpy(resp, "OK|DRAW_REQUESTED");
        } else if (cmd == CMD_DRAW_ACCEPT) {
            strcpy(resp, "OK|DRAW_ACCEPTED");
        } else if (cmd == CMD_DRAW_DECLINE) {
            strcpy(resp, "OK|DRAW_DECLINED");
        } else if (cmd == CMD_REMATCH) {
            strcpy(resp, "OK|REMATCH_REQUESTED");
        } else if (cmd == CMD_REMATCH_ACCEPT) {
            strcpy(resp, "OK|REMATCH_ACCEPTED");
        } else if (cmd == CMD_REMATCH_DECLINE) {
            strcpy(resp, "OK|REMATCH_DECLINED");
        } else {
            strcpy(resp, "OK|ACTION_DONE");
        }
        return resp;
    }

    // Fallback: let CONTROL etc. continue to be handled by legacy mock in main if needed
    char *resp = (char*)malloc(64);
    strcpy(resp, "ERROR|Invalid protocol command format");
    return resp;
}

// ===============================================
// 3. LOGIC PHÂN TÍCH LỆNH (ORCHESTRATOR)
// ===============================================

// Hàm chuyển đổi chuỗi lệnh thành Enum CommandType
CommandType parse_command(const char *command_str) {
    if (strcmp(command_str, "PAUSE") == 0) return CMD_PAUSE;
    if (strcmp(command_str, "RESUME") == 0) return CMD_RESUME;
    if (strcmp(command_str, "SURRENDER") == 0) return CMD_SURRENDER;
    if (strcmp(command_str, "DRAW") == 0) return CMD_DRAW;
    if (strcmp(command_str, "DRAW_ACCEPT") == 0) return CMD_DRAW_ACCEPT;
    if (strcmp(command_str, "DRAW_DECLINE") == 0) return CMD_DRAW_DECLINE;
    if (strcmp(command_str, "REMATCH") == 0) return CMD_REMATCH;
    if (strcmp(command_str, "REMATCH_ACCEPT") == 0) return CMD_REMATCH_ACCEPT;
    if (strcmp(command_str, "REMATCH_DECLINE") == 0) return CMD_REMATCH_DECLINE;
    if (strcmp(command_str, "MODE") == 0) return CMD_MODE_BOT;
    if (strcmp(command_str, "MOVE") == 0) return CMD_MOVE;
    if (strcmp(command_str, "QUIT") == 0) return CMD_QUIT;
    if (strcmp(command_str, "HELLO") == 0) return CMD_HELLO;
    return CMD_UNKNOWN;
}

/**
 * Hàm xử lý chính khi nhận được raw command từ client.
 * @param raw_command: Chuỗi lệnh thô (ví dụ: "MOVE e2e4" hoặc "SURRENDER").
 * @param session: Con trỏ đến phiên game của người chơi hiện tại.
 * @param client_fd: File Descriptor của người gửi lệnh.
 * @param opponent_fd: File Descriptor của đối thủ (hoặc chính nó nếu đấu bot).
 */
void process_incoming_command(const char *raw_command, GameSession *session, int client_fd, int opponent_fd) {
    char command_str[20];
    char params[200] = "";
    CommandType cmd;

    // Phân tích lệnh và tham số
    if (sscanf(raw_command, "%19s %[^\n]", command_str, params) < 1) {
        send_to_client(client_fd, "ERROR INVALID COMMAND FORMAT");
        return;
    }

    cmd = parse_command(command_str);

    // Xử lý các lệnh chung (HELLO, MATCH_FOUND, MOVE trong 1v1) trước
    if (cmd == CMD_HELLO && session->current_state == STATE_WAIT_MATCH) {
        // UC003: Xử lý kết nối và lưu thông tin user.
        send_to_client(client_fd, "WELCOME WAITING");
        // Giả lập đưa vào hàng đợi:
        // Cần logic ghép cặp, sau đó gọi:
        // session->current_state = STATE_IN_GAME;
        // send_to_client(client_fd, "WELCOME WHITE 105"); 
        // send_to_client(opponent_fd, "WELCOME BLACK 105");
        return;
    }
    
    // 1. Điều phối UC002 (Bot Game)
    if (session->is_bot_match || cmd == CMD_MODE_BOT) {
        // Chuyển tiếp lệnh đến module Bot
        handle_bot_game(session, cmd, params, client_fd);
        return;
    }

    // 2. Điều phối UC001 (Match Control)
    if (cmd == CMD_PAUSE || cmd == CMD_RESUME || cmd == CMD_SURRENDER || cmd == CMD_DRAW || 
        cmd == CMD_DRAW_ACCEPT || cmd == CMD_DRAW_DECLINE || cmd == CMD_REMATCH || 
        cmd == CMD_REMATCH_ACCEPT || cmd == CMD_REMATCH_DECLINE) 
    {
        // Chuyển tiếp lệnh đến module Match Control
        handle_match_control(session, cmd, client_fd, opponent_fd);
        return;
    }
    
    // 3. Điều phối UC003 (Move trong 1v1)
    if (session->current_state == STATE_IN_GAME && cmd == CMD_MOVE) {
        char player_move[10];
        sscanf(params, "%s", player_move); 
        
        if (!validate_move(session->current_fen, player_move)) {
            send_to_client(client_fd, "ERROR INVALID MOVE");
            return;
        }
        
        // Cập nhật FEN sau nước đi và lưu DB (Logic này có thể nằm trong một module Game_Logic riêng)
        char fen_after_player_move[256] = "new_fen_string_after_move_uc003";
        strncpy(session->current_fen, fen_after_player_move, sizeof(session->current_fen));
        update_database("INSERT INTO move(...)");
        
        // Phản hồi và Đồng bộ hóa (UC003)
        send_to_client(client_fd, "MOVEOK");
        send_to_client(opponent_fd, "BOARD new_fen_string_after_move_uc003 WHITE/BLACK e2e4"); 

        char status_msg[20];
        if (check_game_end(session->current_fen, status_msg, sizeof(status_msg))) {
             session->current_state = STATE_GAME_OVER;
             char gameover_msg_client[64];
             char gameover_msg_opponent[64];
             snprintf(gameover_msg_client, sizeof(gameover_msg_client), "GAMEOVER %s", status_msg);
             snprintf(gameover_msg_opponent, sizeof(gameover_msg_opponent), "GAMEOVER %s", status_msg);
             send_to_client(client_fd, gameover_msg_client);
             send_to_client(opponent_fd, gameover_msg_opponent);
             update_database("UPDATE match_game SET status='finished'");
        }
        return;
    }

    // Lệnh không xử lý được
    send_to_client(client_fd, "ERROR INVALID COMMAND");
}

