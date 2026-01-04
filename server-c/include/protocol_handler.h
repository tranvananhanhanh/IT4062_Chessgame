#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include "client_session.h"
#include <libpq-fe.h>

// Command handlers
void handle_start_match(ClientSession *session, char *param1, char *param2, 
                       char *param3, char *param4, PGconn *db);
void handle_join_match(ClientSession *session, char *param1, char *param2, 
                      char *param3, PGconn *db);
void handle_move(ClientSession *session, int num_params, char *param1, 
                char *param2, char *param3, char *param4, PGconn *db);
void handle_surrender(ClientSession *session, int num_params, char *param1, 
                     char *param2, PGconn *db);
void handle_get_history(ClientSession *session, char *param1, PGconn *db);
void handle_get_replay(ClientSession *session, char *param1, PGconn *db);
void handle_get_stats(ClientSession *session, char *param1, PGconn *db);
void handle_register(ClientSession *session, char *username, char *password, char *email, PGconn *db);
void handle_login(ClientSession *session, char *username, char *password, PGconn *db);
void handle_get_leaderboard(ClientSession *session, char *limit_param, PGconn *db);

// ✅ Add rematch handlers
void handle_rematch_request(ClientSession *session, int num_params, char *param1, 
                           char *param2, PGconn *db);
void handle_rematch_accept(ClientSession *session, int num_params, char *param1, 
                          char *param2, PGconn *db);
void handle_rematch_decline(ClientSession *session, int num_params, char *param1, 
                           char *param2, PGconn *db);

// Main protocol parser
void protocol_handle_command(ClientSession *session, const char *buffer, PGconn *db);

#endif // PROTOCOL_HANDLER_H
