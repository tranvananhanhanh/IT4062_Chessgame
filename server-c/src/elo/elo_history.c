#include "elo_history.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int elo_save_history(PGconn *db, int user_id, int match_id, int old_elo, int new_elo, int change) {
    if (!db_check_connection(db)) return 0;
    char query[512];
    snprintf(query, sizeof(query),
             "INSERT INTO elo_history (user_id, match_id, old_elo, new_elo, elo_change) "
             "VALUES (%d, %d, %d, %d, %d)",
             user_id, match_id, old_elo, new_elo, change);
    PGresult *res = PQexec(db, query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return 0;
    }
    PQclear(res);
    return 1;
}

int elo_get_history(PGconn *db, int user_id, char *output, int output_size) {
    if (!db_check_connection(db)) {
        snprintf(output, output_size, "ERROR|Database connection failed\n");
        return 0;
    }
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT match_id, old_elo, new_elo, elo_change, "
             "to_char(created_at, 'YYYY-MM-DD HH24:MI') as date "
             "FROM elo_history WHERE user_id = %d "
             "ORDER BY created_at DESC LIMIT 50",
             user_id);
    PGresult *res = PQexec(db, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        snprintf(output, output_size, "ELO_HISTORY|0\n");
        PQclear(res);
        return 1;
    }
    int rows = PQntuples(res);
    int offset = snprintf(output, output_size, "ELO_HISTORY|%d", rows);
    for (int i = 0; i < rows && offset < output_size - 100; i++) {
        const char *match_id = PQgetvalue(res, i, 0);
        const char *old_elo = PQgetvalue(res, i, 1);
        const char *new_elo = PQgetvalue(res, i, 2);
        const char *change = PQgetvalue(res, i, 3);
        const char *date = PQgetvalue(res, i, 4);
        offset += snprintf(output + offset, output_size - offset,
                          "|%s|%s|%s|%s|%s",
                          match_id, old_elo, new_elo, change, date);
    }
    snprintf(output + offset, output_size - offset, "\n");
    PQclear(res);
    return 1;
}
