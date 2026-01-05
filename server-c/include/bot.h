#ifndef BOT_H
#define BOT_H

#include "client_session.h"
#include <libpq-fe.h>
#include <stddef.h>

/**
 * Bot Module - Handles bot game initialization and moves
 */

/**
 * Non-blocking bot request - sends FEN and returns socket fd
 * Return: socket fd to monitor via poll(), or -1 on error
 */
int send_request_to_bot_nonblocking(const char *fen, const char *difficulty);

void handle_mode_bot(
    ClientSession *session,
    char *user_id,
    char *difficulty,
    PGconn *db
);

void handle_bot_move(
    ClientSession *session,
    int num_params,
    char *match_id,
    char *player_move,
    char *difficulty,
    PGconn *db
);

/**
 * Call python chess bot.
 * - fen: current board position (SOURCE OF TRUTH)
 * - difficulty: bot difficulty
 * - bot_move_out: output UCI move (e2e4, g8f6, ...)
 *
 * Return:
 *   0 = OK
 *  -1 = error
 */
int call_python_bot(
    const char *fen,
    const char *difficulty,
    char *bot_move_out,
    size_t bot_move_size
);

#endif // BOT_H
