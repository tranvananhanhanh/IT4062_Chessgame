#ifndef BOT_H
#define BOT_H

#include "client_session.h"
#include <libpq-fe.h>

/**
 * Bot Module - Handles bot game initialization and moves
 */

void handle_mode_bot(ClientSession *session, char *param1, PGconn *db);
void handle_bot_move(ClientSession *session, int num_params, char *param1, 
                    char *param2, char *param3, PGconn *db);

// Helper function to communicate with Python bot
char* call_python_bot(const char *fen, const char *player_move, 
                     char *bot_move_out, size_t bot_move_size);

#endif // BOT_H
