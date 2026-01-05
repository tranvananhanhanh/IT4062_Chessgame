#ifndef MATCHMAKING_H
#define MATCHMAKING_H

#include <stddef.h>
#include "client_session.h"
#include <libpq-fe.h>

int handle_mmjoin(const char *payload, char *out, size_t out_size);
int handle_mmstatus(const char *payload, char *out, size_t out_size);
int handle_mmcancel(const char *payload, char *out, size_t out_size);

// New handlers
void handle_join_matchmaking(ClientSession *session, char *user_id_str, PGconn *db);
void handle_leave_matchmaking(ClientSession *session, char *user_id_str, PGconn *db);

#endif
