#ifndef MATCH_H
#define MATCH_H

#include "client_session.h"
#include <libpq-fe.h>

/**
 * Match Module - Handles PvP match creation, joining, status, moves
 */

// Match lifecycle
void handle_start_match(ClientSession *session, char *param1, char *param2, 
                       char *param3, char *param4, PGconn *db);
void handle_join_match(ClientSession *session, char *param1, char *param2, 
                      char *param3, PGconn *db);
void handle_get_match_status(ClientSession *session, char *param1, PGconn *db);

// Match gameplay
void handle_move(ClientSession *session, int num_params, char *param1, 
                char *param2, char *param3, char *param4, PGconn *db);
void handle_surrender(ClientSession *session, int num_params, char *param1, 
                     char *param2, PGconn *db);

#endif // MATCH_H
