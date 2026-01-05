#include "elo_leaderboard.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int elo_get_leaderboard(PGconn *db, char *output, int output_size, int limit) {
    if (!db_check_connection(db)) {
        snprintf(output, output_size, "ERROR|Database connection failed\n");
        return 0;
    }
    if (limit <= 0 || limit > 100) limit = 20;
    char query[1024];
    snprintf(query, sizeof(query),
             "SELECT u.user_id, u.name, u.elo_point, "
             "COALESCE(SUM(CASE WHEN mp.score = 1 THEN 1 ELSE 0 END), 0) as wins, "
             "COALESCE(SUM(CASE WHEN mp.score = 0 THEN 1 ELSE 0 END), 0) as losses, "
             "COALESCE(SUM(CASE WHEN mp.score = 0.5 THEN 1 ELSE 0 END), 0) as draws "
             "FROM users u "
             "LEFT JOIN match_player mp ON u.user_id = mp.user_id "
             "LEFT JOIN match_game mg ON mp.match_id = mg.match_id AND mg.status = 'finished' AND mg.type = 'pvp' "
             "GROUP BY u.user_id, u.name, u.elo_point "
             "ORDER BY u.elo_point DESC "
             "LIMIT %d",
             limit);
    PGresult *res = PQexec(db, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[ELO] Leaderboard query failed: %s\n", PQerrorMessage(db));
        snprintf(output, output_size, "ERROR|Failed to get leaderboard\n");
        PQclear(res);
        return 0;
    }
    int rows = PQntuples(res);
    int offset = snprintf(output, output_size, "LEADERBOARD|%d", rows);
    for (int i = 0; i < rows && offset < output_size - 150; i++) {
        const char *user_id = PQgetvalue(res, i, 0);
        const char *name = PQgetvalue(res, i, 1);
        const char *elo = PQgetvalue(res, i, 2);
        const char *wins = PQgetvalue(res, i, 3);
        const char *losses = PQgetvalue(res, i, 4);
        const char *draws = PQgetvalue(res, i, 5);
        int rank = i + 1;
        offset += snprintf(output + offset, output_size - offset,
                          "|%d|%s|%s|%s|%s|%s|%s",
                          rank, user_id, name, elo, wins, losses, draws);
    }
    snprintf(output + offset, output_size - offset, "\n");
    PQclear(res);
    return 1;
}
