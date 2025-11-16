#include "database.h"
#include <stdio.h>
#include <string.h>

int db_save_move(PGconn *conn, int match_id, int user_id, 
                const char *notation, const char *fen) {
    if (!db_check_connection(conn)) return 0;
    
    char query[1024];
    char escaped_fen[512];
    
    // Escape FEN string
    PQescapeStringConn(conn, escaped_fen, fen, strlen(fen), NULL);
    
    sprintf(query,
        "INSERT INTO move (match_id, user_id, notation, fen_after, created_at) "
        "VALUES (%d, %d, '%s', '%s', NOW())",
        match_id, user_id, notation, escaped_fen);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "[DB] Save move failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }
    
    PQclear(res);
    return 1;
}

int db_get_match_moves(PGconn *conn, int match_id, char *output, int output_size) {
    if (!db_check_connection(conn)) {
        snprintf(output, output_size, "ERROR|Database connection failed\n");
        return 0;
    }
    
    char query[512];
    sprintf(query,
        "SELECT move_id, user_id, notation, fen_after, created_at "
        "FROM move WHERE match_id = %d ORDER BY move_id ASC",
        match_id);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[DB] Get match moves failed: %s\n", PQerrorMessage(conn));
        snprintf(output, output_size, "ERROR|Failed to get match moves\n");
        PQclear(res);
        return 0;
    }
    
    int rows = PQntuples(res);
    
    if (rows == 0) {
        snprintf(output, output_size, "REPLAY|0\n");
        PQclear(res);
        return 1;
    }
    
    // Format: REPLAY|count|move1|user1|fen1|move2|user2|fen2|...
    int offset = snprintf(output, output_size, "REPLAY|%d", rows);
    
    for (int i = 0; i < rows && offset < output_size - 100; i++) {
        const char *notation = PQgetvalue(res, i, 2);
        const char *user_id = PQgetvalue(res, i, 1);
        const char *fen = PQgetvalue(res, i, 3);
        
        offset += snprintf(output + offset, output_size - offset, 
                          "|%s|%s|%s", notation, user_id, fen);
    }
    
    offset += snprintf(output + offset, output_size - offset, "\n");
    
    PQclear(res);
    return 1;
}
