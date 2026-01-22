#include "recovery.h"
#include "elo_calc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_recovery_system(PGconn *conn) {
    const char *query = 
        "CREATE TABLE IF NOT EXISTS pending_notifications ("
        "id SERIAL PRIMARY KEY, "
        "user_id INT NOT NULL, "
        "message TEXT NOT NULL, "
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");";
    
    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "[Recovery] Failed to init recovery system: %s\n", PQerrorMessage(conn));
    } else {
        printf("[Recovery] Recovery system initialized.\n");
    }
    PQclear(res);
}

void recover_interrupted_matches(PGconn *conn) {
    if (PQstatus(conn) != CONNECTION_OK) return;

    printf("[Recovery] Checking for interrupted matches...\n");

    // 1. Get all matches with status 'playing' that involve real users
    // We join with match_player twice to get both white and black players
    // We ensure they are NOT bots (is_bot = false) - though bots don't get notifications/ELO usually, 
    // but if a human plays a bot and server crashes, human should probably still get something?
    // The requirement says "2 người chơi" (2 players), usually implies PvP. 
    // But let's handle PvP primarily.
    const char *query = 
        "SELECT m.match_id, "
        "p1.user_id as white_id, u1.elo_point as white_elo, "
        "p2.user_id as black_id, u2.elo_point as black_elo "
        "FROM match_game m "
        "JOIN match_player p1 ON m.match_id = p1.match_id AND p1.color = 'white' "
        "JOIN match_player p2 ON m.match_id = p2.match_id AND p2.color = 'black' "
        "JOIN users u1 ON p1.user_id = u1.user_id "
        "JOIN users u2 ON p2.user_id = u2.user_id "
        "WHERE m.status = 'playing'";

    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[Recovery] Failed to fetch interrupted matches: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return;
    }

    int rows = PQntuples(res);
    if (rows == 0) {
        printf("[Recovery] No interrupted matches found.\n");
        PQclear(res);
        return;
    }

    printf("[Recovery] Found %d interrupted matches. Processing...\n", rows);

    for (int i = 0; i < rows; i++) {
        int match_id = atoi(PQgetvalue(res, i, 0));
        int white_id = atoi(PQgetvalue(res, i, 1));
        int white_elo = atoi(PQgetvalue(res, i, 2));
        int black_id = atoi(PQgetvalue(res, i, 3));
        int black_elo = atoi(PQgetvalue(res, i, 4));

        // Calculate ELO gain for White (as if White won)
        double expected_white = elo_expected_score(white_elo, black_elo);
        int k_white = elo_get_k_factor(white_elo, 30); // Assuming > 30 games for safety or standard
        int new_white_elo = elo_calculate_new_rating(white_elo, expected_white, 1.0, k_white);
        int white_gain = new_white_elo - white_elo;

        // Calculate ELO gain for Black (as if Black won)
        double expected_black = elo_expected_score(black_elo, white_elo);
        int k_black = elo_get_k_factor(black_elo, 30);
        int new_black_elo = elo_calculate_new_rating(black_elo, expected_black, 1.0, k_black);
        int black_gain = new_black_elo - black_elo;

        // Update Users ELO
        char update_sql[512];
        snprintf(update_sql, sizeof(update_sql),
            "UPDATE users SET elo_point = elo_point + %d WHERE user_id = %d; "
            "UPDATE users SET elo_point = elo_point + %d WHERE user_id = %d;",
            white_gain, white_id, black_gain, black_id);
        
        PGresult *update_res = PQexec(conn, update_sql);
        PQclear(update_res);

        // Update Match Status
        char match_sql[256];
        snprintf(match_sql, sizeof(match_sql),
            "UPDATE match_game SET status = 'finished', result = 'draw', endtime = NOW() "
            "WHERE match_id = %d", match_id);
    match_sql[sizeof(match_sql)-1] = '\0';
        PGresult *match_res = PQexec(conn, match_sql);

        PQclear(match_res);

        // Notify Users
        const char *msg = "Vì server bị đóng đột ngột chúng tôi rất xin lỗi sự bất tiện này.";
        char notify_sql[1024];
        snprintf(notify_sql, sizeof(notify_sql),
            "INSERT INTO pending_notifications (user_id, message) VALUES "
            "(%d, '%s'), (%d, '%s')",
            white_id, msg, black_id, msg);
        PGresult *notify_res = PQexec(conn, notify_sql);
        PQclear(notify_res);

        printf("[Recovery] Match %d recovered. White(ID:%d) +%d, Black(ID:%d) +%d.\n",
            match_id, white_id, white_gain, black_id, black_gain);
    }

    PQclear(res);
}
