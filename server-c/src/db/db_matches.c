#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int db_create_match(PGconn *conn, int user1_id, int user2_id, const char *type) {
    if (!db_check_connection(conn)) return -1;
    
    // Insert match with 'playing' status - để ván chơi có thể chơi được luôn
    const char *query = "INSERT INTO match_game (type, status, starttime) "
                       "VALUES ($1, 'playing', NOW()) RETURNING match_id";
    
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
    PGresult *res;
    // Chỉ cho phép result là 'white_win', 'black_win', 'draw' (theo constraint)
    // Nếu result là 'white_timeout' hoặc 'black_timeout', chuyển thành hợp lệ
    char safe_result[32];
    if (strcmp(result, "white_timeout") == 0) {
        strcpy(safe_result, "black_win");
    } else if (strcmp(result, "black_timeout") == 0) {
        strcpy(safe_result, "white_win");
    } else {
        strncpy(safe_result, result, sizeof(safe_result)-1);
        safe_result[sizeof(safe_result)-1] = '\0';
    }
    char query[512];
    if (winner_id > 0) {
        sprintf(query,
            "UPDATE match_game SET status='%s', endtime=NOW(), result='%s', winner_id=%d WHERE match_id=%d",
            status, safe_result, winner_id, match_id);
    } else {
        sprintf(query,
            "UPDATE match_game SET status='%s', endtime=NOW(), result='%s' WHERE match_id=%d",
            status, safe_result, match_id);
    }
    res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "[DB] Update match status failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }
    
    PQclear(res);
    
    printf("[DB] Updated match %d status to %s\n", match_id, status);
    
    return 1;
}
