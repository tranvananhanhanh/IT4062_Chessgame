#ifndef FRIEND_H
#define FRIEND_H

#include "client_session.h"
#include <libpq-fe.h>

/**
 * Friend Module - Handles friend system (request, accept, decline, list)
 */

void handle_friend_request(ClientSession *session, int num_params, char *param1, 
                          char *param2, PGconn *db);
void handle_friend_accept(ClientSession *session, int num_params, char *param1, 
                         char *param2, PGconn *db);
void handle_friend_decline(ClientSession *session, int num_params, char *param1, 
                          char *param2, PGconn *db);
void handle_friend_list(ClientSession *session, int num_params, char *param1, PGconn *db);
void handle_friend_requests(ClientSession *session, int num_params, char *param1, PGconn *db);

#endif // FRIEND_H
