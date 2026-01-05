#include "login.h"
#include "database.h"
#include "online_users.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h> // for atoi

// Khai báo prototype cho simple_hash để tránh lỗi implicit declaration
unsigned long long simple_hash(const char *str);

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

    // Check if user is already logged in
    extern OnlineUsers online_users;
    pthread_mutex_lock(&online_users.lock);
    for (int i = 0; i < online_users.count; ++i) {
        if (online_users.entries[i].user_id == user_id) {
            pthread_mutex_unlock(&online_users.lock);
            char error[] = "ERROR|User already logged in\n";
            send(session->socket_fd, error, strlen(error), 0);
            printf("[Login] User %d already logged in, rejecting duplicate login\n", user_id);
            return;
        }
    }
    pthread_mutex_unlock(&online_users.lock);

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

    // Add user to online_users
    online_users_add(&online_users, user_id, name, session->socket_fd);
    
    // Update user state to online in database
    char update_query[256];
    snprintf(update_query, sizeof(update_query), 
        "UPDATE users SET state = 'online' WHERE user_id = %d", user_id);
    PGresult *res = PQexec(db, update_query);
    PQclear(res);

    char response[256];
    snprintf(response, sizeof(response), "LOGIN_SUCCESS|%d|%s|%s\n", user_id, name, elo);
    send(session->socket_fd, response, strlen(response), 0);
}

void handle_register_validate(ClientSession *session, int num_params, char *param1, char *param2, char *param3, PGconn *db) {
    (void)num_params; // suppress unused parameter warning
    // param1: username, param2: password, param3: email (optional)
    const char *username = param1;
    const char *password = param2;
    const char *email = param3 ? param3 : "";

    // Kiểm tra username đã tồn tại chưa
    char query[256];
    snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE name = '%s'", username);
    PGresult *res = PQexec(db, query);
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        PQclear(res);
        const char *resp = "REGISTER_ERROR|Username already exists\n";
        send(session->socket_fd, resp, strlen(resp), 0);
        return;
    }
    PQclear(res);

    // Hash password (simple)
    unsigned long long hashval = simple_hash(password);
    char hashstr[32];
    snprintf(hashstr, sizeof(hashstr), "%016llx", hashval);

    // Thêm user mới với password_hash
    snprintf(query, sizeof(query),
        "INSERT INTO users (name, password_hash, email) VALUES ('%s', '%s', '%s') RETURNING user_id",
        username, hashstr, email);
    res = PQexec(db, query);
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        int user_id = atoi(PQgetvalue(res, 0, 0));
        PQclear(res);
        char resp[128];
        snprintf(resp, sizeof(resp), "REGISTER_OK|%d\n", user_id);
        send(session->socket_fd, resp, strlen(resp), 0);
    } else {
        fprintf(stderr, "[Register] DB error: %s\n", PQerrorMessage(db));
        PQclear(res);
        const char *resp = "REGISTER_ERROR|Database error\n";
        send(session->socket_fd, resp, strlen(resp), 0);
    }
}
