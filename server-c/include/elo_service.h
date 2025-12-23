#ifndef ELO_SERVICE_H
#define ELO_SERVICE_H

#include <libpq-fe.h>

// ELO Constants
#define ELO_DEFAULT 1000
#define ELO_K_FACTOR_NEW 40      // K-factor for players with < 30 games
#define ELO_K_FACTOR_NORMAL 20   // K-factor for regular players
#define ELO_K_FACTOR_MASTER 10   // K-factor for players with ELO > 2400
#define ELO_MIN 100              // Minimum ELO rating

// Game result scores
#define SCORE_WIN 1.0
#define SCORE_DRAW 0.5
#define SCORE_LOSS 0.0

// ELO Change structure
typedef struct {
    int player_id;
    int old_elo;
    int new_elo;
    int elo_change;
} EloChange;

// ELO calculation functions
double elo_expected_score(int player_elo, int opponent_elo);
int elo_get_k_factor(int elo, int games_played);
int elo_calculate_new_rating(int current_elo, double expected, double actual, int k_factor);

// Database ELO operations
int elo_get_player_rating(PGconn *db, int user_id);
int elo_get_player_games_count(PGconn *db, int user_id);
int elo_update_ratings(PGconn *db, int winner_id, int loser_id, EloChange *winner_change, EloChange *loser_change);
int elo_update_draw(PGconn *db, int player1_id, int player2_id, EloChange *p1_change, EloChange *p2_change);

// ELO history
int elo_save_history(PGconn *db, int user_id, int match_id, int old_elo, int new_elo, int change);
int elo_get_history(PGconn *db, int user_id, char *output, int output_size);

// Leaderboard
int elo_get_leaderboard(PGconn *db, char *output, int output_size, int limit);

#endif // ELO_SERVICE_H
