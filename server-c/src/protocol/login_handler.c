#include "login_handler.h"
#include "database.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

void handle_login(ClientSession *session, char *param1, char *param2, PGconn *db) {
    if (param1 == NULL || param2 == NULL || strlen(param1) == 0 || strlen(param2) == 0) {
        char error[] = "ERROR|Missing credentials\n";
        send(session->socket_fd, error, strlen(error), 0);
        return;
    }

    int user_id = db_verify_user(db, param1, param2);
    if (user_id <= 0) {
        char error[] = "ERROR|Invalid credentials\n";
        send(session->socket_fd, error, strlen(error), 0);
        return;
    }

    char user_info[256];
    if (!db_get_user_info(db, user_id, user_info, sizeof(user_info))) {
        char error[] = "ERROR|Failed to load user info\n";
        send(session->socket_fd, error, strlen(error), 0);
        return;
    }

    // user_info format: USER_INFO|name|elo\n
    char name[128] = {0};
    char elo[64] = {0};
    sscanf(user_info, "USER_INFO|%127[^|]|%63s", name, elo);

    session->user_id = user_id;
    strncpy(session->username, name, sizeof(session->username) - 1);

    char response[256];
    snprintf(response, sizeof(response), "LOGIN_SUCCESS|%d|%s|%s\n", user_id, name, elo);
    send(session->socket_fd, response, strlen(response), 0);
}
