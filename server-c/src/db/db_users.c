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

    // NOTE: storing raw password into password_hash for demo purposes only
    snprintf(query, sizeof(query),
        "INSERT INTO users (name, password_hash, elo_point) "
        "VALUES ('%s', '%s', 1200) RETURNING user_id",
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

int db_get_top_players(PGconn *conn, int limit, char *output, int output_size) {
    if (!db_check_connection(conn)) {
        snprintf(output, output_size, "ERROR|Database connection failed\n");
        return 0;
    }

    char query[1024];
    snprintf(query, sizeof(query),
        "SELECT u.name, COALESCE(u.elo_point,1200) AS elo, "
        "COALESCE(SUM(CASE WHEN mp.score = 1 THEN 1 ELSE 0 END),0) AS wins, "
        "COALESCE(SUM(CASE WHEN mp.score = 0 THEN 1 ELSE 0 END),0) AS losses, "
        "COALESCE(SUM(CASE WHEN mp.score = 0.5 THEN 1 ELSE 0 END),0) AS draws "
        "FROM users u "
        "LEFT JOIN match_player mp ON mp.user_id = u.user_id "
        "GROUP BY u.user_id, u.name, u.elo_point "
        "ORDER BY elo DESC, u.name ASC "
        "LIMIT %d",
        limit);

    PGresult *res = PQexec(conn, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[DB] Get leaderboard failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }

    int rows = PQntuples(res);
    int offset = snprintf(output, output_size, "LEADERBOARD_DATA");

    for (int i = 0; i < rows && offset < output_size - 64; i++) {
        const char *name = PQgetvalue(res, i, 0);
        const char *elo = PQgetvalue(res, i, 1);
        const char *wins = PQgetvalue(res, i, 2);
        const char *losses = PQgetvalue(res, i, 3);
        const char *draws = PQgetvalue(res, i, 4);

        offset += snprintf(output + offset, output_size - offset,
                           "|%d,%s,%s,%s,%s,%s", i + 1, name, elo, wins, losses, draws);
    }

    snprintf(output + offset, output_size - offset, "\n");
    PQclear(res);
    return 1;
}
