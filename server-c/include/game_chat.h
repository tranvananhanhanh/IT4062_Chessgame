#ifndef GAME_CHAT_H
#define GAME_CHAT_H

#include "client_session.h"
#include <libpq-fe.h>

// Handle in-game chat during match
// Format: GAME_CHAT|match_id|message
void handle_game_chat(ClientSession *session, int match_id, const char *message, PGconn *db);

#endif // GAME_CHAT_H
