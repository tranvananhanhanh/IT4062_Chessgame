#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int db_create_user(PGconn *conn, const char *username, const char *password) {
    if (!db_check_connection(conn)) return 0;
    
    char query[1024];
    char escaped_username[256];
    char escaped_password[256];
    
    // Escape strings to prevent SQL injection
    PQescapeStringConn(conn, escaped_username, username, strlen(username), NULL);
    PQescapeStringConn(conn, escaped_password, password, strlen(password), NULL);
    
    snprintf(query, sizeof(query),
        "INSERT INTO users (name, password_hash, elo_point) "
        "VALUES ('%s', '%s', 0) RETURNING user_id",
        escaped_username, escaped_password);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[DB] Create user failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }
    
    int user_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    printf("[DB] Created user '%s' with ID %d\n", username, user_id);
    return user_id;
}

int db_verify_user(PGconn *conn, const char *username, const char *password) {
    if (!db_check_connection(conn)) return 0;
    
    char query[1024];
    char escaped_username[256];
    char escaped_password[256];
    
    PQescapeStringConn(conn, escaped_username, username, strlen(username), NULL);
    PQescapeStringConn(conn, escaped_password, password, strlen(password), NULL);
    
    snprintf(query, sizeof(query),
        "SELECT user_id FROM users WHERE name = '%s' AND password_hash = '%s'",
        escaped_username, escaped_password);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return 0; // Authentication failed
    }
    
    int user_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    return user_id;
}

int db_get_user_info(PGconn *conn, int user_id, char *output, int output_size) {
    if (!db_check_connection(conn)) {
        snprintf(output, output_size, "ERROR|Database connection failed\n");
        return 0;
    }
    
    char query[1024];
    sprintf(query,
        "SELECT name, elo_point FROM users WHERE user_id = %d",
        user_id);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        snprintf(output, output_size, "ERROR|User not found\n");
        PQclear(res);
        return 0;
    }
    
    const char *username = PQgetvalue(res, 0, 0);
    const char *elo = PQgetvalue(res, 0, 1);
    
    snprintf(output, output_size, "USER_INFO|%s|%s\n", username, elo);
    PQclear(res);
    
    return 1;
}
