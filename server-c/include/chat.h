#ifndef CHAT_H
#define CHAT_H

#include "client_session.h"
#include <libpq-fe.h>

// Xử lý lệnh CHAT|to_user|message
void handle_chat(ClientSession *session, int num_params, const char *to_user, const char *message);

#endif
