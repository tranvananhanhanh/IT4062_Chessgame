#include "elo_db.h"
#include "database.h"
#include "elo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int elo_get_player_rating(PGconn *db, int user_id) {
    if (!db_check_connection(db)) return ELO_DEFAULT;
    char query[256];
    snprintf(query, sizeof(query), "SELECT elo_point FROM users WHERE user_id = %d", user_id);
    PGresult *res = PQexec(db, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return ELO_DEFAULT;
    }
    int elo = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return elo;
}

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

int elo_update_ratings(PGconn *db, int winner_id, int loser_id, EloChange *winner_change, EloChange *loser_change) {
    if (!db_check_connection(db)) return 0;
    int winner_elo = elo_get_player_rating(db, winner_id);
    int loser_elo = elo_get_player_rating(db, loser_id);
    int winner_games = elo_get_player_games_count(db, winner_id);
    int loser_games = elo_get_player_games_count(db, loser_id);
    double winner_expected = elo_expected_score(winner_elo, loser_elo);
    double loser_expected = elo_expected_score(loser_elo, winner_elo);
    int winner_k = elo_get_k_factor(winner_elo, winner_games);
    int loser_k = elo_get_k_factor(loser_elo, loser_games);
    int winner_new_elo = elo_calculate_new_rating(winner_elo, winner_expected, 1.0, winner_k);
    int loser_new_elo = elo_calculate_new_rating(loser_elo, loser_expected, 0.0, loser_k);
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
    char query[512];
    snprintf(query, sizeof(query), "UPDATE users SET elo_point = %d WHERE user_id = %d", winner_new_elo, winner_id);
    PGresult *res = PQexec(db, query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return 0;
    }
    PQclear(res);
    snprintf(query, sizeof(query), "UPDATE users SET elo_point = %d WHERE user_id = %d", loser_new_elo, loser_id);
    res = PQexec(db, query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return 0;
    }
    PQclear(res);
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
    return 1;
}

int elo_update_draw(PGconn *db, int player1_id, int player2_id, EloChange *p1_change, EloChange *p2_change) {
    if (!db_check_connection(db)) return 0;
    int p1_elo = elo_get_player_rating(db, player1_id);
    int p2_elo = elo_get_player_rating(db, player2_id);
    int p1_games = elo_get_player_games_count(db, player1_id);
    int p2_games = elo_get_player_games_count(db, player2_id);
    double p1_expected = elo_expected_score(p1_elo, p2_elo);
    double p2_expected = elo_expected_score(p2_elo, p1_elo);
    int p1_k = elo_get_k_factor(p1_elo, p1_games);
    int p2_k = elo_get_k_factor(p2_elo, p2_games);
    int p1_new_elo = elo_calculate_new_rating(p1_elo, p1_expected, 0.5, p1_k);
    int p2_new_elo = elo_calculate_new_rating(p2_elo, p2_expected, 0.5, p2_k);
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
    char query[512];
    snprintf(query, sizeof(query), "UPDATE users SET elo_point = %d WHERE user_id = %d", p1_new_elo, player1_id);
    PGresult *res = PQexec(db, query);
    PQclear(res);
    snprintf(query, sizeof(query), "UPDATE users SET elo_point = %d WHERE user_id = %d", p2_new_elo, player2_id);
    res = PQexec(db, query);
    PQclear(res);
    snprintf(query, sizeof(query),
        "UPDATE match_player SET score = 0.5 WHERE user_id IN (%d, %d) AND match_id = "
        "(SELECT match_id FROM match_game WHERE result = 'draw' ORDER BY endtime DESC LIMIT 1)",
        player1_id, player2_id);
    PQexec(db, query);
    return 1;
}
