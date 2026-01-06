// bot.c - Bot game handlers (NON-BLOCKING EVENT-DRIVEN VERSION)

#include "bot.h"
#include "game.h"
#include "history.h"
#include "client_session.h"    // ← THÊM DÒNG NÀY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

extern GameManager game_manager;

/* ================= SEND REQUEST TO BOT (NON-BLOCKING) ================= */
// Hàm gửi request sang bot dạng non-blocking, trả về socket fd
int send_request_to_bot_nonblocking(const char *fen, const char *difficulty) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5001);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    char request[2048];
    snprintf(request, sizeof(request), "%s|%s\n", fen, difficulty);
    send(sock, request, strlen(request), 0);

    // Không nhận ở đây!  Trả socket fd để poll
    return sock;
}

/* ================= MODE BOT (Khởi tạo trận với bot) ================= */
void handle_mode_bot(ClientSession *session, char *user_id, char *difficulty, PGconn *db) {
    int user_id_int = atoi(user_id);

    Player white = {user_id_int, session->socket_fd, COLOR_WHITE, 1, ""};
    snprintf(white.username, sizeof(white.username), "Player_%d", user_id_int);

    Player black = {0, -1, COLOR_BLACK, 1, "AI_Bot"};
    strcpy(black.username, "AI_Bot");

    GameMatch *match = game_manager_create_bot_match(
        &game_manager, white, black, db, difficulty
    );

    if (! match) {
        send(session->socket_fd,
             "ERROR|Failed to create bot match\n", 33, 0);
        return;
    }

    session->user_id = user_id_int;
    session->current_match = match;

    char resp[512];
    snprintf(resp, sizeof(resp),
             "BOT_MATCH_CREATED|%d|%s\n",
             match->match_id, match->board.fen);

    send(session->socket_fd, resp, strlen(resp), 0);
}

/* ================= APPLY PLAYER MOVE (helper) ================= */
// Áp dụng nước đi của người chơi lên board, trả về 0 nếu thất bại
int apply_player_move_on_board(GameMatch *match, const char *player_move, int user_id, PGconn *db) {
    if (strlen(player_move) < 4) {
        printf("[ERROR] Move too short:  %s\n", player_move);
        return 0;
    }

    char from[3] = { player_move[0], player_move[1], '\0' };
    char to[3]   = { player_move[2], player_move[3], '\0' };

    Move pm = {0};
    notation_to_coords(from, &pm.from_row, &pm. from_col);
    notation_to_coords(to, &pm.to_row, &pm. to_col);

    pm.piece = match->board.board[pm.from_row][pm. from_col];
    pm. captured_piece = match->board. board[pm.to_row][pm.to_col];

    // Parse promotion suffix nếu có
    pm.is_promotion = 0;
    pm.promotion_piece = '\0';
    if (strlen(player_move) == 5) {
        char suffix = player_move[4];
        if (suffix == 'q' || suffix == 'r' || suffix == 'b' || suffix == 'n') {
            pm.promotion_piece = (suffix == 'q') ? 'Q' : (suffix == 'r') ? 'R' : 
                                 (suffix == 'b') ? 'B' : 'N';
            char piece_type = toupper(pm.piece);
            int last_rank = 0;  // Trắng đến rank 8 (row 0)
            if (piece_type == 'P' && pm.to_row == last_rank) {
                pm.is_promotion = 1;
                printf("[DEBUG] Promotion:  %s -> %c\n", player_move, pm.promotion_piece);
            } else {
                printf("[WARN] Invalid promotion:  %s\n", player_move);
            }
        }
    }

    if (!validate_move(&match->board, &pm, COLOR_WHITE)) {
        printf("[ERROR] Invalid player move:  %s\n", player_move);
        return 0;
    }

    execute_move(&match->board, &pm);

    // Lưu lịch sử
    char fen_after_player[FEN_MAX_LENGTH];
    chess_board_to_fen(&match->board, fen_after_player);
    history_save_move(db, match->match_id, user_id, player_move, fen_after_player);

    return 1;
}

/* ================= APPLY BOT MOVE (helper - gọi ở event-loop) ================= */
// Áp dụng nước đi bot từ event-loop khi nhận được kết quả từ bot server
void apply_bot_move_on_board(GameMatch *match, const char *bot_move, PGconn *db) {
    if (strcmp(bot_move, "NOMOVE") == 0) {
        match->result = RESULT_DRAW;
        match->status = GAME_FINISHED;
        return;
    }

    if (strcmp(bot_move, "ERROR") == 0 || strlen(bot_move) < 4) {
        printf("[ERROR] Bot returned invalid move: %s\n", bot_move);
        match->result = RESULT_WHITE_WIN;
        match->status = GAME_FINISHED;
        return;
    }

    char bfrom[3] = { bot_move[0], bot_move[1], '\0' };
    char bto[3]   = { bot_move[2], bot_move[3], '\0' };

    Move bm = {0};
    notation_to_coords(bfrom, &bm.from_row, &bm.from_col);
    notation_to_coords(bto, &bm. to_row, &bm. to_col);

    bm.piece = match->board.board[bm.from_row][bm.from_col];
    bm.captured_piece = match->board.board[bm.to_row][bm. to_col];

    // Parse promotion cho bot (lowercase cho đen)
    bm.is_promotion = 0;
    bm.promotion_piece = '\0';
    if (strlen(bot_move) == 5) {
        char suffix = bot_move[4];
        if (suffix == 'q' || suffix == 'r' || suffix == 'b' || suffix == 'n') {
            bm.promotion_piece = suffix;  // lowercase cho đen
            char piece_type = toupper(bm. piece);
            int last_rank_black = 7;
            if (piece_type == 'P' && bm.to_row == last_rank_black) {
                bm.is_promotion = 1;
                printf("[DEBUG] Bot promotion:  %s -> %c\n", bot_move, bm.promotion_piece);
            } else {
                printf("[WARN] Invalid bot promotion: %s\n", bot_move);
            }
        }
    }

    if (!validate_move(&match->board, &bm, COLOR_BLACK)) {
        printf("[ERROR] Invalid bot move:  %s\n", bot_move);
        match->status = GAME_FINISHED;
        match->result = RESULT_WHITE_WIN;
        return;
    }

    execute_move(&match->board, &bm);

    // Lưu lịch sử
    char fen_after_bot[FEN_MAX_LENGTH];
    chess_board_to_fen(&match->board, fen_after_bot);
    history_save_bot_move(db, match->match_id, bot_move, fen_after_bot);
}
