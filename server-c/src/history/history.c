#include "history.h"
#include "database.h"
#include "game.h"
#include "elo_service.h"
#include <string.h>

// These are wrapper functions that call the database layer
// This keeps the existing history.h API intact while delegating to the new database modules

// Create new match in database
int history_create_match(PGconn *conn, int user1_id, int user2_id, const char *type) {
    return db_create_match(conn, user1_id, user2_id, type);
}

// Create bot match in database with NULL user_id for bot
int history_create_bot_match(PGconn *conn, int user_id, const char *type) {
    return db_create_bot_match(conn, user_id, type);
}

// Save move to database
int history_save_move(PGconn *conn, int match_id, int user_id, const char *notation, const char *fen) {
    return db_save_move(conn, match_id, user_id, notation, fen);
}

// Save bot move to database with NULL user_id
int history_save_bot_move(PGconn *conn, int match_id, const char *notation, const char *fen) {
    return db_save_bot_move(conn, match_id, notation, fen);
}

// Update match result
int history_update_match_result(PGconn *conn, int match_id, const char *result, int winner_id) {
    return db_update_match_status(conn, match_id, "finished", result, winner_id);
}

// Get user match history
// NOTE: This function keeps the complex JOIN logic here since it involves game-specific
// data formatting and opponent information
int history_get_user_matches(PGconn *conn, int user_id, MatchHistoryItem *items, int *count) {
    if (!db_check_connection(conn)) return 0;
    
    char query[1024];
    sprintf(query,
        "SELECT mg.match_id, "
        "u.user_id as opponent_id, u.name as opponent_name, "
        "mg.result, mp1.color, "
        "to_char(mg.starttime, 'YYYY-MM-DD HH24:MI:SS') as start_time, "
        "to_char(mg.endtime, 'YYYY-MM-DD HH24:MI:SS') as end_time, "
        "(SELECT COUNT(*) FROM move WHERE match_id = mg.match_id) as move_count "
        "FROM match_game mg "
        "JOIN match_player mp1 ON mg.match_id = mp1.match_id AND mp1.user_id = %d "
        "JOIN match_player mp2 ON mg.match_id = mp2.match_id AND mp2.user_id != %d "
        "JOIN users u ON mp2.user_id = u.user_id "
        "WHERE mg.status = 'finished' "
        "ORDER BY mg.starttime DESC LIMIT 20",
        user_id, user_id);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[DB] Get history failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }
    
    int rows = PQntuples(res);
    *count = rows;
    
    for (int i = 0; i < rows; i++) {
        items[i].match_id = atoi(PQgetvalue(res, i, 0));
        items[i].opponent_id = atoi(PQgetvalue(res, i, 1));
        strcpy(items[i].opponent_name, PQgetvalue(res, i, 2));
        
        // Determine result from player's perspective
        const char *db_result = PQgetvalue(res, i, 3);
        const char *player_color = PQgetvalue(res, i, 4);
        
        if (strcmp(db_result, "draw") == 0 || strcmp(db_result, "stalemate") == 0) {
            strcpy(items[i].result, "draw");
        } else if ((strcmp(db_result, "white_win") == 0 && strcmp(player_color, "white") == 0) ||
                   (strcmp(db_result, "black_win") == 0 && strcmp(player_color, "black") == 0)) {
            strcpy(items[i].result, "win");
        } else {
            strcpy(items[i].result, "loss");
        }
        
        strcpy(items[i].player_color, player_color);
        strcpy(items[i].start_time, PQgetvalue(res, i, 5));
        strcpy(items[i].end_time, PQgetvalue(res, i, 6));
        items[i].move_count = atoi(PQgetvalue(res, i, 7));
    }
    
    PQclear(res);
    
    printf("[DB] Retrieved %d match history items for user %d\n", rows, user_id);
    
    return 1;
}

// Get replay moves
int replay_get_moves(PGconn *conn, int match_id, MoveRecord *moves, int *count) {
    if (!db_check_connection(conn)) return 0;
    
    char query[512];
    sprintf(query,
        "SELECT move_id, user_id, notation, fen_after, "
        "to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as created_at "
        "FROM move WHERE match_id = %d ORDER BY move_id ASC",
        match_id);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[DB] Get replay failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }
    
    int rows = PQntuples(res);
    *count = rows;
    
    for (int i = 0; i < rows && i < MAX_MOVES_PER_GAME; i++) {
        moves[i].move_id = atoi(PQgetvalue(res, i, 0));
        moves[i].user_id = atoi(PQgetvalue(res, i, 1));
        strcpy(moves[i].notation, PQgetvalue(res, i, 2));
        strcpy(moves[i].fen_after, PQgetvalue(res, i, 3));
        strcpy(moves[i].created_at, PQgetvalue(res, i, 4));
    }
    
    PQclear(res);
    
    printf("[DB] Retrieved %d moves for replay of match %d\n", rows, match_id);
    
    return 1;
}

// Validate user can access replay
int replay_validate_access(PGconn *conn, int match_id, int user_id) {
    if (!db_check_connection(conn)) return 0;
    
    char query[256];
    sprintf(query,
        "SELECT COUNT(*) FROM match_player "
        "WHERE match_id = %d AND user_id = %d",
        match_id, user_id);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 0;
    }
    
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    return count > 0;
}

// Get player statistics
int stats_get_player_stats(PGconn *conn, int user_id, PlayerStats *stats) {
    if (!db_check_connection(conn)) return 0;
    
    char query[1024];
    sprintf(query,
        "SELECT "
        "COUNT(*) as total, "
        "SUM(CASE WHEN mp.score = 1 THEN 1 ELSE 0 END) as wins, "
        "SUM(CASE WHEN mp.score = 0 THEN 1 ELSE 0 END) as losses, "
        "SUM(CASE WHEN mp.score = 0.5 THEN 1 ELSE 0 END) as draws, "
        "u.elo_point "
        "FROM match_player mp "
        "JOIN match_game mg ON mp.match_id = mg.match_id "
        "JOIN users u ON mp.user_id = u.user_id "
        "WHERE mp.user_id = %d AND mg.status = 'finished' "
        "GROUP BY u.elo_point",
        user_id);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        // No games played yet
        stats->user_id = user_id;
        stats->total_games = 0;
        stats->wins = 0;
        stats->losses = 0;
        stats->draws = 0;
        stats->win_rate = 0.0;
        stats->current_elo = 1000;
        PQclear(res);
        return 1;
    }
    
    stats->user_id = user_id;
    stats->total_games = atoi(PQgetvalue(res, 0, 0));
    stats->wins = atoi(PQgetvalue(res, 0, 1));
    stats->losses = atoi(PQgetvalue(res, 0, 2));
    stats->draws = atoi(PQgetvalue(res, 0, 3));
    stats->current_elo = atoi(PQgetvalue(res, 0, 4));
    
    if (stats->total_games > 0) {
        stats->win_rate = (float)stats->wins / stats->total_games * 100.0;
    } else {
        stats->win_rate = 0.0;
    }
    
    PQclear(res);
    
    printf("[DB] Retrieved stats for user %d: %d games, %d wins\n", 
           user_id, stats->total_games, stats->wins);
    
    return 1;
}

// Update ELO ratings using proper ELO calculation
int stats_update_elo(PGconn *conn, int winner_id, int loser_id) {
    if (!db_check_connection(conn)) return 0;
    
    EloChange winner_change, loser_change;
    
    int result = elo_update_ratings(conn, winner_id, loser_id, &winner_change, &loser_change);
    
    if (result) {
        printf("[DB] Updated ELO: winner %d (%d -> %d, %+d), loser %d (%d -> %d, %+d)\n", 
               winner_id, winner_change.old_elo, winner_change.new_elo, winner_change.elo_change,
               loser_id, loser_change.old_elo, loser_change.new_elo, loser_change.elo_change);
    }
    
    return result;
}

// Update ELO for draw games
int stats_update_elo_draw(PGconn *conn, int player1_id, int player2_id) {
    if (!db_check_connection(conn)) return 0;
    
    EloChange p1_change, p2_change;
    
    int result = elo_update_draw(conn, player1_id, player2_id, &p1_change, &p2_change);
    
    if (result) {
        printf("[DB] Updated ELO for draw: player %d (%d -> %d, %+d), player %d (%d -> %d, %+d)\n", 
               player1_id, p1_change.old_elo, p1_change.new_elo, p1_change.elo_change,
               player2_id, p2_change.old_elo, p2_change.new_elo, p2_change.elo_change);
    }
    
    return result;
}
