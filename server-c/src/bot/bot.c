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
// Gửi request sang bot server và trả về socket fd để poll
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

    // Không recv ở đây – trả về socket để event-loop đọc
    return sock;
}


void handle_mode_bot(ClientSession *session, char *user_id, char *difficulty, PGconn *db) {
    int user_id_int = atoi(user_id);

    Player white = {user_id_int, session->socket_fd, COLOR_WHITE, 1, ""};
    snprintf(white.username, sizeof(white.username), "Player_%d", user_id_int);

    Player black = {0, -1, COLOR_BLACK, 1, "AI_Bot"};
    strcpy(black.username, "AI_Bot");

    GameMatch *match = game_manager_create_bot_match(
        &game_manager, white, black, db, difficulty
    );

    if (!match) {
        send(session->socket_fd,
             "ERROR|Failed to create bot match\n", 33, 0);
        return;
    }

    /* ✅ SET user_id để handle_bot_move có thể dùng */
    session->user_id = user_id_int;
    session->current_match = match;

    char resp[512];
    snprintf(resp, sizeof(resp),
             "BOT_MATCH_CREATED|%d|%s\n",
             match->match_id, match->board.fen);

    send(session->socket_fd, resp, strlen(resp), 0);
}

/* ================= APPLY PLAYER MOVE (helper) ================= */
// Áp dụng nước đi của người chơi, kèm fallback promotion nếu thiếu suffix
int apply_player_move_on_board(GameMatch *match, const char *player_move, int user_id, PGconn *db) {
    if (strlen(player_move) < 4) {
        printf("[ERROR] Move too short: %s\n", player_move);
        return 0;
    }

    char from[3] = { player_move[0], player_move[1], '\0' };
    char to[3]   = { player_move[2], player_move[3], '\0' };

    Move pm = {0};
    notation_to_coords(from, &pm.from_row, &pm.from_col);
    notation_to_coords(to, &pm.to_row, &pm.to_col);

    pm.piece = match->board.board[pm.from_row][pm.from_col];
    pm.captured_piece = match->board.board[pm.to_row][pm.to_col];

    pm.is_promotion = 0;
    pm.promotion_piece = '\0';
    if (strlen(player_move) == 5) {
        char suffix = player_move[4];
        if (suffix == 'q' || suffix == 'r' || suffix == 'b' || suffix == 'n') {
            pm.promotion_piece = (suffix == 'q') ? 'Q' : (suffix == 'r') ? 'R' :
                                 (suffix == 'b') ? 'B' : 'N';
            int last_rank = 0;
            if (toupper(pm.piece) == 'P' && pm.to_row == last_rank) {
                pm.is_promotion = 1;
                printf("[DEBUG] Promotion: %s -> %c\n", player_move, pm.promotion_piece);
            } else {
                printf("[WARN] Invalid promotion: %s\n", player_move);
            }
        }
    }

    if (!validate_move(&match->board, &pm, COLOR_WHITE)) {
        printf("[ERROR] Invalid player move: %s\n", player_move);
        return 0;
    }

    // Fallback: nếu quên suffix nhưng tốt đã tới hàng cuối thì tự phong hậu
    if (!pm.is_promotion && toupper(pm.piece) == 'P' &&
        (pm.to_row == 0 || pm.to_row == 7)) {
        pm.is_promotion = 1;
        pm.promotion_piece = is_white_piece(pm.piece) ? WHITE_QUEEN : BLACK_QUEEN;
    }

    execute_move(&match->board, &pm);

    char fen_after_player[FEN_MAX_LENGTH];
    chess_board_to_fen(&match->board, fen_after_player);
    history_save_move(db, match->match_id, user_id, player_move, fen_after_player);

    return 1;
}

/* ================= APPLY BOT MOVE (helper - event loop sẽ gọi) ================= */
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
    notation_to_coords(bto, &bm.to_row, &bm.to_col);

    bm.piece = match->board.board[bm.from_row][bm.from_col];
    bm.captured_piece = match->board.board[bm.to_row][bm.to_col];

    bm.is_promotion = 0;
    bm.promotion_piece = '\0';
    if (strlen(bot_move) == 5) {
        char suffix = bot_move[4];
        if (suffix == 'q' || suffix == 'r' || suffix == 'b' || suffix == 'n') {
            bm.promotion_piece = suffix;  // lowercase cho đen
            int last_rank_black = 7;
            if (toupper(bm.piece) == 'P' && bm.to_row == last_rank_black) {
                bm.is_promotion = 1;
                printf("[DEBUG] Bot promotion: %s -> %c\n", bot_move, bm.promotion_piece);
            } else {
                printf("[WARN] Invalid bot promotion: %s\n", bot_move);
            }
        }
    }

    if (!validate_move(&match->board, &bm, COLOR_BLACK)) {
        printf("[ERROR] Invalid bot move: %s\n", bot_move);
        match->status = GAME_FINISHED;
        match->result = RESULT_WHITE_WIN;
        return;
    }

    if (!bm.is_promotion && toupper(bm.piece) == 'P' &&
        (bm.to_row == 0 || bm.to_row == 7)) {
        bm.is_promotion = 1;
        bm.promotion_piece = is_white_piece(bm.piece) ? WHITE_QUEEN : BLACK_QUEEN;
    }

    execute_move(&match->board, &bm);

    char fen_after_bot[FEN_MAX_LENGTH];
    chess_board_to_fen(&match->board, fen_after_bot);
    history_save_bot_move(db, match->match_id, bot_move, fen_after_bot);
}

/* ================= CALL PYTHON BOT ================= */
/*
Protocol:
    C -> Python : "fen|difficulty\n"
    Python -> C : "e2e4\n" | "NOMOVE\n" | "ERROR\n"
*/

int call_python_bot(
    const char *fen,
    const char *difficulty,
    char *bot_move_out,
    size_t bot_move_size
) {
    const char *host = getenv("BOT_HOST") ? getenv("BOT_HOST") : "127.0.0.1";
    int port = getenv("BOT_PORT") ? atoi(getenv("BOT_PORT")) : 5001;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[Bot] socket()");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    char request[2048];
    snprintf(request, sizeof(request), "%s|%s\n", fen, difficulty);
    send(sock, request, strlen(request), 0);

    char response[128];
    ssize_t n = recv(sock, response, sizeof(response) - 1, 0);
    close(sock);

    if (n <= 0) return -1;

    response[n] = '\0';

    char *nl = strchr(response, '\n');
    if (nl) *nl = '\0';

    strncpy(bot_move_out, response, bot_move_size - 1);
    bot_move_out[bot_move_size - 1] = '\0';

    return 0;
}

/* ================= BOT MOVE ================= */

void handle_bot_move(
    ClientSession *session,
    int num_params,
    char *param1,
    char *param2,
    char *param3,
    PGconn *db
) {
    if (num_params < 4 || !session) return;

    int match_id = atoi(param1);
    char *player_move = param2;  // e.g., "e7e8q" (full UCI)
    char *difficulty = param3;

    GameMatch *match = game_manager_find_match(&game_manager, match_id);
    if (!match) return;

    pthread_mutex_lock(&match->lock);

    if (match->status != GAME_PLAYING ||
        match->board.current_turn != COLOR_WHITE) {
        pthread_mutex_unlock(&match->lock);
        return;
    }

    /* ===== PLAYER MOVE (helper) ===== */
    if (!apply_player_move_on_board(match, player_move, session->user_id, db)) {
        pthread_mutex_unlock(&match->lock);
        return;
    }

    /* ===== BOT MOVE ===== */
    char fen_before_bot[FEN_MAX_LENGTH];
    chess_board_to_fen(&match->board, fen_before_bot);

    char bot_move[16] = "ERROR";
    if (call_python_bot(fen_before_bot, difficulty,
                        bot_move, sizeof(bot_move)) == 0) {
        apply_bot_move_on_board(match, bot_move, db);
    } else {
        // Bot không trả về – xem như lỗi
        apply_bot_move_on_board(match, "ERROR", db);
    }

    /* ===== FINAL STATE ===== */
    chess_board_to_fen(&match->board, match->board.fen);
    game_match_check_end_condition(match, db);

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

    send(session->socket_fd, resp, strlen(resp), 0);

    pthread_mutex_unlock(&match->lock);
}
