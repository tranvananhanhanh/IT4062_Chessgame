// protocol_handler.h - Protocol command router header
#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include "client_session.h"
#include <libpq-fe.h>

// Main protocol parser (only public interface)
void protocol_handle_command(ClientSession *session, const char *buffer, PGconn *db);

// Handler modules (imported by protocol_handler.c)
// - match.h: handle_start_match, handle_join_match, handle_get_match_status, handle_move, handle_surrender
// - friend.h: handle_friend_request, handle_friend_accept, handle_friend_decline, handle_friend_list, handle_friend_requests
// - bot.h: handle_mode_bot, handle_bot_move
// - control.h: handle_pause, handle_resume, handle_draw_*, handle_rematch_*
// - history.h: handle_get_history, handle_get_replay, handle_get_stats

#endif // PROTOCOL_HANDLER_H
