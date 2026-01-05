#include "database.h"
#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int db_get_user_matches(PGconn *conn, int user_id, char *output, int output_size) {
    if (!db_check_connection(conn)) {
        snprintf(output, output_size, "ERROR|Database connection failed\n");
        return 0;
    }
    
    char query[1024];
    sprintf(query,
        "SELECT mg.match_id, mg.type, mg.status, mg.result, mp.color, "
        "to_char(mg.starttime, 'YYYY-MM-DD HH24:MI:SS') as starttime, "
        "to_char(mg.endtime, 'YYYY-MM-DD HH24:MI:SS') as endtime, "
        "(SELECT COUNT(*) FROM move WHERE match_id = mg.match_id) as move_count "
        "FROM match_game mg "
        "JOIN match_player mp ON mg.match_id = mp.match_id "
        "WHERE mp.user_id = %d "
        "ORDER BY mg.starttime DESC LIMIT 50",
        user_id);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[DB] Get user matches failed: %s\n", PQerrorMessage(conn));
        snprintf(output, output_size, "ERROR|Failed to get match history\n");
        PQclear(res);
        return 0;
    }
    
    int rows = PQntuples(res);
    
    if (rows == 0) {
        snprintf(output, output_size, "HISTORY|0\n");
        PQclear(res);
        return 1;
    }
    
    // Format: HISTORY|count|match1_data|match2_data|...
    int offset = snprintf(output, output_size, "HISTORY|%d", rows);
    
    for (int i = 0; i < rows && offset < output_size - 200; i++) {
        const char *match_id = PQgetvalue(res, i, 0);
        const char *type = PQgetvalue(res, i, 1);
        const char *status = PQgetvalue(res, i, 2);
        const char *result = PQgetvalue(res, i, 3);
        const char *color = PQgetvalue(res, i, 4);
        const char *start = PQgetvalue(res, i, 5);
        const char *end = PQgetvalue(res, i, 6);
        const char *moves = PQgetvalue(res, i, 7);
        
        offset += snprintf(output + offset, output_size - offset,
                          "|%s|%s|%s|%s|%s|%s|%s|%s",
                          match_id, type, status, result, color, start, end, moves);
    }
    
    offset += snprintf(output + offset, output_size - offset, "\n");
    
    PQclear(res);
    return 1;
}

int db_get_user_stats(PGconn *conn, int user_id, char *output, int output_size) {
    if (!db_check_connection(conn)) {
        snprintf(output, output_size, "ERROR|Database connection failed\n");
        return 0;
    }
    
    char query[1024];
    sprintf(query,
        "SELECT "
        "COUNT(*) as total, "
        "SUM(CASE WHEN mp.score = 1 THEN 1 ELSE 0 END) as wins, "
        "SUM(CASE WHEN mp.score = 0 THEN 1 ELSE 0 END) as losses, "
        "SUM(CASE WHEN mp.score = 0.5 THEN 1 ELSE 0 END) as draws, "
        "COALESCE(u.elo_point, 1200) as elo "
        "FROM match_player mp "
        "JOIN match_game mg ON mp.match_id = mg.match_id "
        "LEFT JOIN users u ON mp.user_id = u.user_id "
        "WHERE mp.user_id = %d AND mg.status = 'finished' "
        "GROUP BY u.elo_point",
        user_id);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        // No matches yet, return default stats
        snprintf(output, output_size, "STATS|0|0|0|0|1200\n");
        PQclear(res);
        return 1;
    }
    
    const char *total = PQgetvalue(res, 0, 0);
    const char *wins = PQgetvalue(res, 0, 1);
    const char *losses = PQgetvalue(res, 0, 2);
    const char *draws = PQgetvalue(res, 0, 3);
    const char *elo = PQgetvalue(res, 0, 4);
    
    snprintf(output, output_size, "STATS|%s|%s|%s|%s|%s\n",
             total, wins, losses, draws, elo);
    
    PQclear(res);
    return 1;
}

int db_update_user_stats(PGconn *conn, int user_id, const char *result) {
    if (!db_check_connection(conn)) return 0;
    
    // This is a simplified version - in production, you'd update ELO rating
    char query[512];
    
    if (strcmp(result, "win") == 0) {
        sprintf(query,
            "UPDATE users SET elo_point = elo_point + 25 WHERE user_id = %d",
            user_id);
    } else if (strcmp(result, "loss") == 0) {
        sprintf(query,
            "UPDATE users SET elo_point = GREATEST(elo_point - 25, 0) WHERE user_id = %d",
            user_id);
    } else {
        // Draw - no change or small adjustment
        return 1;
    }
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "[DB] Update user stats failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }
    
    PQclear(res);
    return 1;
}
