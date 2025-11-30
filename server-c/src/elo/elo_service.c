#include "elo_service.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ==================== ELO CALCULATION FUNCTIONS ====================

/**
 * Calculate expected score using standard ELO formula
 * E = 1 / (1 + 10^((opponent_elo - player_elo) / 400))
 */
double elo_expected_score(int player_elo, int opponent_elo) {
    double exponent = (double)(opponent_elo - player_elo) / 400.0;
    return 1.0 / (1.0 + pow(10.0, exponent));
}

/**
 * Get K-factor based on player's ELO and number of games played
 * - New players (< 30 games): K = 40 (more volatile)
 * - Regular players: K = 20
 * - Master players (ELO > 2400): K = 10 (more stable)
 */
int elo_get_k_factor(int elo, int games_played) {
    if (games_played < 30) {
        return ELO_K_FACTOR_NEW;
    } else if (elo > 2400) {
        return ELO_K_FACTOR_MASTER;
    } else {
        return ELO_K_FACTOR_NORMAL;
    }
}

/**
 * Calculate new ELO rating after a game
 * new_rating = old_rating + K * (actual_score - expected_score)
 */
int elo_calculate_new_rating(int current_elo, double expected, double actual, int k_factor) {
    double change = k_factor * (actual - expected);
    int new_elo = current_elo + (int)round(change);
    
    // Ensure minimum ELO
    if (new_elo < ELO_MIN) {
        new_elo = ELO_MIN;
    }
    
    return new_elo;
}

// ==================== DATABASE ELO OPERATIONS ====================

/**
 * Get player's current ELO rating from database
 */
int elo_get_player_rating(PGconn *db, int user_id) {
    if (!db_check_connection(db)) return ELO_DEFAULT;
    
    char query[256];
    snprintf(query, sizeof(query), 
             "SELECT elo_point FROM users WHERE user_id = %d", user_id);
    
    PGresult *res = PQexec(db, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return ELO_DEFAULT;
    }
    
    int elo = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    return elo;
}

/**
 * Get total number of games played by a player
 */
int elo_get_player_games_count(PGconn *db, int user_id) {
    if (!db_check_connection(db)) return 0;
    
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT COUNT(*) FROM match_player mp "
             "JOIN match_game mg ON mp.match_id = mg.match_id "
             "WHERE mp.user_id = %d AND mg.status = 'finished' AND mg.type = 'pvp'",
             user_id);
    
    PGresult *res = PQexec(db, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return 0;
    }
    
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    return count;
}

/**
 * Update ELO ratings after a match with a winner and loser
 */
int elo_update_ratings(PGconn *db, int winner_id, int loser_id, 
                       EloChange *winner_change, EloChange *loser_change) {
    if (!db_check_connection(db)) return 0;
    
    // Get current ratings
    int winner_elo = elo_get_player_rating(db, winner_id);
    int loser_elo = elo_get_player_rating(db, loser_id);
    
    // Get games count for K-factor
    int winner_games = elo_get_player_games_count(db, winner_id);
    int loser_games = elo_get_player_games_count(db, loser_id);
    
    // Calculate expected scores
    double winner_expected = elo_expected_score(winner_elo, loser_elo);
    double loser_expected = elo_expected_score(loser_elo, winner_elo);
    
    // Get K-factors
    int winner_k = elo_get_k_factor(winner_elo, winner_games);
    int loser_k = elo_get_k_factor(loser_elo, loser_games);
    
    // Calculate new ratings
    int winner_new_elo = elo_calculate_new_rating(winner_elo, winner_expected, SCORE_WIN, winner_k);
    int loser_new_elo = elo_calculate_new_rating(loser_elo, loser_expected, SCORE_LOSS, loser_k);
    
    // Fill EloChange structures
    if (winner_change) {
        winner_change->player_id = winner_id;
        winner_change->old_elo = winner_elo;
        winner_change->new_elo = winner_new_elo;
        winner_change->elo_change = winner_new_elo - winner_elo;
    }
    
    if (loser_change) {
        loser_change->player_id = loser_id;
        loser_change->old_elo = loser_elo;
        loser_change->new_elo = loser_new_elo;
        loser_change->elo_change = loser_new_elo - loser_elo;
    }
    
    // Update database
    char query[512];
    
    // Update winner ELO
    snprintf(query, sizeof(query),
             "UPDATE users SET elo_point = %d WHERE user_id = %d",
             winner_new_elo, winner_id);
    
    PGresult *res = PQexec(db, query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "[ELO] Failed to update winner ELO: %s\n", PQerrorMessage(db));
        PQclear(res);
        return 0;
    }
    PQclear(res);
    
    // Update loser ELO
    snprintf(query, sizeof(query),
             "UPDATE users SET elo_point = %d WHERE user_id = %d",
             loser_new_elo, loser_id);
    
    res = PQexec(db, query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "[ELO] Failed to update loser ELO: %s\n", PQerrorMessage(db));
        PQclear(res);
        return 0;
    }
    PQclear(res);
    
    // Update match_player scores
    snprintf(query, sizeof(query),
             "UPDATE match_player SET score = 1 WHERE user_id = %d AND match_id = "
             "(SELECT match_id FROM match_game WHERE winner_id = %d ORDER BY endtime DESC LIMIT 1)",
             winner_id, winner_id);
    PQexec(db, query);
    
    snprintf(query, sizeof(query),
             "UPDATE match_player SET score = 0 WHERE user_id = %d AND match_id = "
             "(SELECT match_id FROM match_game WHERE winner_id = %d ORDER BY endtime DESC LIMIT 1)",
             loser_id, winner_id);
    PQexec(db, query);
    
    printf("[ELO] Updated ratings - Winner %d: %d -> %d (+%d), Loser %d: %d -> %d (%d)\n",
           winner_id, winner_elo, winner_new_elo, winner_new_elo - winner_elo,
           loser_id, loser_elo, loser_new_elo, loser_new_elo - loser_elo);
    
    // Save ELO history
    // Get latest match_id for these players
    char match_query[256];
    snprintf(match_query, sizeof(match_query),
             "SELECT match_id FROM match_game WHERE winner_id = %d ORDER BY endtime DESC LIMIT 1",
             winner_id);
    
    PGresult *match_res = PQexec(db, match_query);
    int match_id = 0;
    if (PQresultStatus(match_res) == PGRES_TUPLES_OK && PQntuples(match_res) > 0) {
        match_id = atoi(PQgetvalue(match_res, 0, 0));
    }
    PQclear(match_res);
    
    if (match_id > 0) {
        elo_save_history(db, winner_id, match_id, winner_elo, winner_new_elo, winner_new_elo - winner_elo);
        elo_save_history(db, loser_id, match_id, loser_elo, loser_new_elo, loser_new_elo - loser_elo);
    }
    
    return 1;
}

/**
 * Update ELO ratings after a draw
 */
int elo_update_draw(PGconn *db, int player1_id, int player2_id, 
                    EloChange *p1_change, EloChange *p2_change) {
    if (!db_check_connection(db)) return 0;
    
    // Get current ratings
    int p1_elo = elo_get_player_rating(db, player1_id);
    int p2_elo = elo_get_player_rating(db, player2_id);
    
    // Get games count for K-factor
    int p1_games = elo_get_player_games_count(db, player1_id);
    int p2_games = elo_get_player_games_count(db, player2_id);
    
    // Calculate expected scores
    double p1_expected = elo_expected_score(p1_elo, p2_elo);
    double p2_expected = elo_expected_score(p2_elo, p1_elo);
    
    // Get K-factors
    int p1_k = elo_get_k_factor(p1_elo, p1_games);
    int p2_k = elo_get_k_factor(p2_elo, p2_games);
    
    // Calculate new ratings (both get SCORE_DRAW = 0.5)
    int p1_new_elo = elo_calculate_new_rating(p1_elo, p1_expected, SCORE_DRAW, p1_k);
    int p2_new_elo = elo_calculate_new_rating(p2_elo, p2_expected, SCORE_DRAW, p2_k);
    
    // Fill EloChange structures
    if (p1_change) {
        p1_change->player_id = player1_id;
        p1_change->old_elo = p1_elo;
        p1_change->new_elo = p1_new_elo;
        p1_change->elo_change = p1_new_elo - p1_elo;
    }
    
    if (p2_change) {
        p2_change->player_id = player2_id;
        p2_change->old_elo = p2_elo;
        p2_change->new_elo = p2_new_elo;
        p2_change->elo_change = p2_new_elo - p2_elo;
    }
    
    // Update database
    char query[512];
    
    snprintf(query, sizeof(query),
             "UPDATE users SET elo_point = %d WHERE user_id = %d",
             p1_new_elo, player1_id);
    PGresult *res = PQexec(db, query);
    PQclear(res);
    
    snprintf(query, sizeof(query),
             "UPDATE users SET elo_point = %d WHERE user_id = %d",
             p2_new_elo, player2_id);
    res = PQexec(db, query);
    PQclear(res);
    
    // Update match_player scores (both 0.5 for draw)
    snprintf(query, sizeof(query),
             "UPDATE match_player SET score = 0.5 WHERE user_id IN (%d, %d) AND match_id = "
             "(SELECT match_id FROM match_game WHERE result = 'draw' ORDER BY endtime DESC LIMIT 1)",
             player1_id, player2_id);
    PQexec(db, query);
    
    printf("[ELO] Draw - Player %d: %d -> %d (%+d), Player %d: %d -> %d (%+d)\n",
           player1_id, p1_elo, p1_new_elo, p1_new_elo - p1_elo,
           player2_id, p2_elo, p2_new_elo, p2_new_elo - p2_elo);
    
    // Save ELO history for draw
    char match_query[256];
    snprintf(match_query, sizeof(match_query),
             "SELECT match_id FROM match_game WHERE result = 'draw' ORDER BY endtime DESC LIMIT 1");
    
    PGresult *match_res = PQexec(db, match_query);
    int match_id = 0;
    if (PQresultStatus(match_res) == PGRES_TUPLES_OK && PQntuples(match_res) > 0) {
        match_id = atoi(PQgetvalue(match_res, 0, 0));
    }
    PQclear(match_res);
    
    if (match_id > 0) {
        elo_save_history(db, player1_id, match_id, p1_elo, p1_new_elo, p1_new_elo - p1_elo);
        elo_save_history(db, player2_id, match_id, p2_elo, p2_new_elo, p2_new_elo - p2_elo);
    }
    
    return 1;
}

// ==================== ELO HISTORY ====================

/**
 * Save ELO change to history table
 */
int elo_save_history(PGconn *db, int user_id, int match_id, int old_elo, int new_elo, int change) {
    if (!db_check_connection(db)) return 0;
    
    char query[512];
    snprintf(query, sizeof(query),
             "INSERT INTO elo_history (user_id, match_id, old_elo, new_elo, elo_change) "
             "VALUES (%d, %d, %d, %d, %d)",
             user_id, match_id, old_elo, new_elo, change);
    
    PGresult *res = PQexec(db, query);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        // Table might not exist, silently fail
        PQclear(res);
        return 0;
    }
    
    PQclear(res);
    return 1;
}

/**
 * Get ELO history for a player
 * Output format: ELO_HISTORY|count|match_id1|old_elo1|new_elo1|change1|date1|...
 */
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

// ==================== LEADERBOARD ====================

/**
 * Get ELO leaderboard (top players)
 * Output format: LEADERBOARD|count|rank1|name1|elo1|wins1|losses1|draws1|...
 */
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
