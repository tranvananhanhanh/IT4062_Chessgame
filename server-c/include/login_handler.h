#ifndef LOGIN_HANDLER_H
#define LOGIN_HANDLER_H

#include "client_session.h"
#include <libpq-fe.h>

// Login handler
void handle_login(ClientSession *session, char *param1, char *param2, PGconn *db);

#endif // LOGIN_HANDLER_H
