#include "login.h"
#include <string.h>
#include <stdio.h>
#include <libpq-fe.h>
#include <sys/socket.h>
#include <stdlib.h>

// Khai báo prototype cho simple_hash để tránh lỗi implicit declaration
unsigned long long simple_hash(const char *str);

void handle_register(ClientSession *session, char *param1, char *param2, char *param3, PGconn *db) {
    // param1: username, param2: password, param3: email (optional)
    const char *username = param1;
    const char *password = param2;
    const char *email = param3 ? param3 : "";

    // Kiểm tra username đã tồn tại chưa
    char query[512];
    char escaped_username[256];
    PQescapeStringConn(db, escaped_username, username, strlen(username), NULL);
    
    snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE name = '%s'", escaped_username);
    
    PGresult *res = PQexec(db, query);
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        PQclear(res);
        const char *resp = "REGISTER_ERROR|Username already exists\n";
        send(session->socket_fd, resp, strlen(resp), 0);
        return;
    }
    PQclear(res);

    // Thêm user mới - hash password
    unsigned long long hashval = simple_hash(password);
    char hashstr[32];
    snprintf(hashstr, sizeof(hashstr), "%016llx", hashval);
    
    char escaped_email[256];
    PQescapeStringConn(db, escaped_email, email, strlen(email), NULL);
    
    snprintf(query, sizeof(query),
        "INSERT INTO users (name, password_hash, email) VALUES ('%s', '%s', '%s') RETURNING user_id",
        escaped_username, hashstr, escaped_email);
    
    res = PQexec(db, query);
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        int user_id = atoi(PQgetvalue(res, 0, 0));
        PQclear(res);
        char resp[128];
        snprintf(resp, sizeof(resp), "REGISTER_OK|%d\n", user_id);
        send(session->socket_fd, resp, strlen(resp), 0);
    } else {
        PQclear(res);
        const char *resp = "REGISTER_ERROR|Database error\n";
        send(session->socket_fd, resp, strlen(resp), 0);
    }
}
