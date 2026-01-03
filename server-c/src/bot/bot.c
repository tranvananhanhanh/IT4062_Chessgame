// bot.c - Bot game handlers (FIXED & SAFE VERSION - WITH PROMOTION SUPPORT)

#include "bot.h"
#include "game.h"
#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

extern GameManager game_manager;

/* ================= MODE BOT ================= */

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

    /* ===== PARSE PLAYER MOVE SAFELY WITH PROMOTION ===== */
    if (strlen(player_move) < 4) {
        pthread_mutex_unlock(&match->lock);
        return;
    }

    char from[3] = { player_move[0], player_move[1], '\0' };
    char to[3]   = { player_move[2], player_move[3], '\0' };

    Move pm = {0};
    notation_to_coords(from, &pm.from_row, &pm.from_col);
    notation_to_coords(to, &pm.to_row, &pm.to_col);

    pm.piece = match->board.board[pm.from_row][pm.from_col];
    pm.captured_piece = match->board.board[pm.to_row][pm.to_col];

    // ← FIX: Parse promotion suffix nếu có (length ==5, last char in q/r/b/n)
    pm.is_promotion = 0;  // Default
    pm.promotion_piece = '\0';
    if (strlen(player_move) == 5) {
        char suffix = player_move[4];
        if (suffix == 'q' || suffix == 'r' || suffix == 'b' || suffix == 'n') {
            // Upper cho trắng (user trắng)
            pm.promotion_piece = (suffix == 'q') ? 'Q' : (suffix == 'r') ? 'R' : 
                                 (suffix == 'b') ? 'B' : 'N';
            // Validate: Chỉ pawn đến last rank (row 0 cho trắng)
            char piece_type = toupper(pm.piece);
            int last_rank = 0;  // Trắng đến rank 8 (row 0)
            if (piece_type == 'P' && pm.to_row == last_rank) {
                pm.is_promotion = 1;
                printf("[DEBUG] Promotion detected: %s -> promote to %c\n", player_move, pm.promotion_piece);
            } else {
                printf("[WARN] Invalid promotion UCI for player: %s (not pawn or wrong rank)\n", player_move);
            }
        }
    }

    if (!validate_move(&match->board, &pm, COLOR_WHITE)) {
    printf("[ERROR] Invalid player move rejected: %s\n", player_move);
    pthread_mutex_unlock(&match->lock);
    return;
}


    execute_move(&match->board, &pm);
    
    // Save player move with FEN after move (lưu full UCI)
    char fen_after_player[FEN_MAX_LENGTH];
    chess_board_to_fen(&match->board, fen_after_player);
    history_save_move(db, match_id, session->user_id, player_move, fen_after_player);

    /* ===== BOT MOVE ===== */
    char fen_before_bot[FEN_MAX_LENGTH];
    chess_board_to_fen(&match->board, fen_before_bot);

    char bot_move[16] = {0};
    if (call_python_bot(fen_before_bot, difficulty,
                        bot_move, sizeof(bot_move)) == 0 &&
        strlen(bot_move) >= 4 &&
        strcmp(bot_move, "NOMOVE") != 0 &&
        strcmp(bot_move, "ERROR") != 0) {

        char bfrom[3] = { bot_move[0], bot_move[1], '\0' };
        char bto[3]   = { bot_move[2], bot_move[3], '\0' };

        Move bm = {0};
        notation_to_coords(bfrom, &bm.from_row, &bm.from_col);
        notation_to_coords(bto, &bm.to_row, &bm.to_col);

        bm.piece = match->board.board[bm.from_row][bm.from_col];
        bm.captured_piece = match->board.board[bm.to_row][bm.to_col];

        // ← FIX: Bot cũng có thể promote (nếu UCI 5 char, suffix lowercase cho đen)
        bm.is_promotion = 0;
        bm.promotion_piece = '\0';
        if (strlen(bot_move) == 5) {
            char suffix = bot_move[4];
            if (suffix == 'q' || suffix == 'r' || suffix == 'b' || suffix == 'n') {
                // Lowercase cho đen (bot đen)
                bm.promotion_piece = (suffix == 'q') ? 'q' : (suffix == 'r') ? 'r' : 
                                     (suffix == 'b') ? 'b' : 'n';
                // Validate: Pawn đen đến rank 1 (row 7)
                char piece_type = toupper(bm.piece);
                int last_rank_black = 7;
                if (piece_type == 'P' && bm.to_row == last_rank_black) {  // 'p' uppercase 'P'
                    bm.is_promotion = 1;
                    printf("[DEBUG] Bot promotion detected: %s -> promote to %c\n", bot_move, bm.promotion_piece);
                } else {
                    printf("[WARN] Invalid bot promotion UCI: %s\n", bot_move);
                }
            }
        }
        if (!validate_move(&match->board, &bm, COLOR_BLACK)) {
    printf("[ERROR] Invalid bot move rejected: %s\n", bot_move);

    // Bot đi sai → coi như bot thua
    match->status = GAME_FINISHED;
    match->result = RESULT_WHITE_WIN;

    pthread_mutex_unlock(&match->lock);
    return;
}

execute_move(&match->board, &bm);


        
        // Save bot move with FEN after bot's move
        char fen_after_bot[FEN_MAX_LENGTH];
        chess_board_to_fen(&match->board, fen_after_bot);
        history_save_bot_move(db, match_id, bot_move, fen_after_bot);
    } else if (strcmp(bot_move, "NOMOVE") == 0) {
        // Handle stalemate/draw nếu bot no move
        match->result = RESULT_DRAW;
        match->status = GAME_FINISHED;
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
