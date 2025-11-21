// UC002: Xử lý Giao thức Chơi với Bot (Play with Bot)

#include <stdio.h>
#include <stdlib.h>
#include "include/protocol_common.h"

// Khai báo hàm mock để đảm bảo liên kết
void send_to_client(int client_fd, const char *message);
void update_database(const char *query);
char* call_ai_bot(const char *fen, char *ai_move_buffer, size_t buffer_size);
bool validate_move(const char *fen, const char *move);
bool check_game_end(const char *new_fen, char *result_status, size_t status_size);


/**
 * Xử lý UC002: Chơi với Bot (Play with Bot)
 * Xử lý luồng MODE BOT, MOVE và QUIT.
 */
void handle_bot_game(GameSession *session, CommandType cmd, const char *params, int client_fd) {
    printf("[UC002] Handling command %d in state %d\n", cmd, session->current_state);

    if (cmd == CMD_MODE_BOT) {
        if (session->current_state == STATE_WAIT_MODE) {
            session->is_bot_match   = true;
            session->current_state  = STATE_IN_GAME;
            strncpy(session->current_fen,
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                    sizeof(session->current_fen));
            session->current_fen[sizeof(session->current_fen) - 1] = '\0';

            // Trả start FEN theo format mà Flask mong đợi
            char response[512];
            snprintf(response, sizeof(response),
                     "OK|%s|NONE|MODE BOT READY", session->current_fen);
            send_to_client(client_fd, response);
            return;
        }
    }

    if (session->current_state == STATE_IN_GAME) {
        if (cmd == CMD_MOVE) {
            char player_move[10];
            sscanf(params, "%9s", player_move);

            if (!validate_move(session->current_fen, player_move)) {
                send_to_client(client_fd, "ERROR|INVALID_MOVE");
                return;
            }

            // Gọi AI: nhận "fen_after_player|fen_after_bot"
            char bot_move[10] = {0};
            char *fen_pair = call_ai_bot(session->current_fen, bot_move, sizeof(bot_move));
            if (fen_pair == NULL) {
                send_to_client(client_fd, "ERROR|AI_FAILED");
                return;
            }

            // tách fen_after_player|fen_after_bot
            char *sep = strchr(fen_pair, '|');
            if (!sep) {
                free(fen_pair);
                send_to_client(client_fd, "ERROR|AI_BAD_FORMAT");
                return;
            }
            *sep = '\0';
            const char *fen_after_player = fen_pair;
            const char *fen_after_bot    = sep + 1;

            // cập nhật session->current_fen = fen_after_bot
            strncpy(session->current_fen, fen_after_bot, sizeof(session->current_fen));
            session->current_fen[sizeof(session->current_fen) - 1] = '\0';

            char status_msg[32] = "IN_GAME";
            if (check_game_end(session->current_fen, status_msg, sizeof(status_msg))) {
                session->current_state = STATE_GAME_OVER;
            }

            // Response format: OK|fen_after_player|bot_move|fen_after_bot|status
            char response[1024];
            snprintf(response, sizeof(response),
                     "OK|%s|%s|%s|%s",
                     fen_after_player,
                     bot_move[0] ? bot_move : "NONE",
                     fen_after_bot,
                     status_msg);

            send_to_client(client_fd, response);
            free(fen_pair);
            return;
        }

        if (cmd == CMD_QUIT) {
            session->current_state = STATE_TERMINATED;
            send_to_client(client_fd, "BYE");
            return;
        }
    }

    // Xử lý lỗi chung (INVALID COMMAND)
    send_to_client(client_fd, "ERROR INVALID COMMAND");
}
