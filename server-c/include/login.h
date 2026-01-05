#ifndef LOGIN_H
#define LOGIN_H

#include "client_session.h"
#include <libpq-fe.h>

// Handle user login
void handle_login(ClientSession *session, char *param1, char *param2, PGconn *db);
void handle_logout(ClientSession *session);
void handle_register(ClientSession *session, char *param1, char *param2, char *param3, PGconn *db);
void handle_register_validate(ClientSession *session, int num_params, char *param1, char *param2, char *param3, PGconn *db);

#endif // LOGIN_H
