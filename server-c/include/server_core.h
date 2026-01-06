#ifndef SERVER_CORE_H
#define SERVER_CORE_H

#include <libpq-fe.h>

#define PORT 8888
#define MAX_CLIENTS 100

void register_bot_request(int game_id, const char *fen, const char *difficulty);
// Server initialization and lifecycle
int server_init(PGconn **db_conn);
void server_start();
void server_shutdown();

// Client handling
void* client_handler(void *arg);

#endif // SERVER_CORE_H
