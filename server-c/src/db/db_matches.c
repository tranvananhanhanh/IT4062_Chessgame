#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int db_create_match(PGconn *conn, int user1_id, int user2_id, const char *type) {
    if (!db_check_connection(conn)) return -1;
    
    // Insert match with 'waiting' status - will be updated to 'playing' when opponent joins
    const char *query = "INSERT INTO match_game (type, status, starttime) "
                       "VALUES ($1, 'waiting', NOW()) RETURNING match_id";
    
    const char *values[1] = {type};
    PGresult *res = PQexecParams(conn, query, 1, NULL, values, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[DB] Create match failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    
    int match_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    // Insert match players
    char insert_players[512];
    sprintf(insert_players,
        "INSERT INTO match_player (match_id, user_id, color, is_bot) VALUES "
        "(%d, %d, 'white', false), (%d, %d, 'black', false)",
        match_id, user1_id, match_id, user2_id);
    
    res = PQexec(conn, insert_players);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "[DB] Insert players failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    
    PQclear(res);
    
    printf("[DB] Created match %d: user %d vs user %d\n", match_id, user1_id, user2_id);
    
    return match_id;
}

// Create a bot match with NULL user_id for bot player
int db_create_bot_match(PGconn *conn, int user_id, const char *type) {
    if (!db_check_connection(conn)) return -1;
    
    // Insert match
    const char *query = "INSERT INTO match_game (type, status, starttime) "
                       "VALUES ($1, 'playing', NOW()) RETURNING match_id";
    
    const char *values[1] = {type};
    PGresult *res = PQexecParams(conn, query, 1, NULL, values, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[DB] Create bot match failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    
    int match_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    // Insert match players: human player (white) and bot (black with NULL user_id)
    char insert_players[512];
    sprintf(insert_players,
        "INSERT INTO match_player (match_id, user_id, color, is_bot) VALUES "
        "(%d, %d, 'white', false), (%d, NULL, 'black', true)",
        match_id, user_id, match_id);
    
    res = PQexec(conn, insert_players);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "[DB] Insert bot match players failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    
    PQclear(res);
    
    printf("[DB] Created bot match %d: user %d vs AI Bot\n", match_id, user_id);
    
    return match_id;
}

int db_update_match_status(PGconn *conn, int match_id, const char *status, 
                           const char *result, int winner_id) {
    if (!db_check_connection(conn)) return 0;
    
    char query[512];
    if (winner_id > 0) {
        sprintf(query,
            "UPDATE match_game SET status='%s', endtime=NOW(), "
            "result='%s', winner_id=%d WHERE match_id=%d",
            status, result, winner_id, match_id);
    } else {
        sprintf(query,
            "UPDATE match_game SET status='%s', endtime=NOW(), "
            "result='%s' WHERE match_id=%d",
            status, result, match_id);
    }
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "[DB] Update match status failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }
    
    PQclear(res);
    
    // Update match_player scores
    char update_scores[512];
    if (strcmp(result, "white_win") == 0) {
        sprintf(update_scores,
            "UPDATE match_player SET score = CASE "
            "WHEN color = 'white' THEN 1.0 "
            "WHEN color = 'black' THEN 0.0 END "
            "WHERE match_id = %d", match_id);
    } else if (strcmp(result, "black_win") == 0) {
        sprintf(update_scores,
            "UPDATE match_player SET score = CASE "
            "WHEN color = 'black' THEN 1.0 "
            "WHEN color = 'white' THEN 0.0 END "
            "WHERE match_id = %d", match_id);
    } else if (strcmp(result, "draw") == 0) {
        sprintf(update_scores,
            "UPDATE match_player SET score = 0.5 WHERE match_id = %d", match_id);
    }
    
    res = PQexec(conn, update_scores);
    PQclear(res);
    
    return 1;
}

int db_get_match_status(PGconn *conn, int match_id, char *status, int status_size) {
    if (!db_check_connection(conn)) return 0;
    
    char query[256];
    sprintf(query, "SELECT status FROM match_game WHERE match_id = %d", match_id);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return 0;
    }
    
    strncpy(status, PQgetvalue(res, 0, 0), status_size - 1);
    status[status_size - 1] = '\0';
    
    PQclear(res);
    return 1;
}
