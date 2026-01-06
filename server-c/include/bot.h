#ifndef BOT_H
#define BOT_H

#include <libpq-fe.h>
#include "client_session.h"
#include "game.h"

// Gửi request non-blocking tới bot server
int send_request_to_bot_nonblocking(const char *fen, const char *difficulty);

// Khởi tạo trận bot
void handle_mode_bot(ClientSession *session, char *user_id, char *difficulty, PGconn *db);

// Áp dụng nước đi người chơi lên board
int apply_player_move_on_board(GameMatch *match, const char *player_move, int user_id, PGconn *db);

// Áp dụng nước đi bot lên board (gọi từ event-loop)
void apply_bot_move_on_board(GameMatch *match, const char *bot_move, PGconn *db);

// Đăng ký bot request vào poll pool
void register_bot_request(int game_id, const char *fen, const char *difficulty);

#endif
