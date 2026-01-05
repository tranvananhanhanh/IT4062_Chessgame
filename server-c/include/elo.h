#ifndef ELO_H
#define ELO_H

#include <libpq-fe.h>

#define ELO_DEFAULT 1200

typedef struct {
    int player_id;
    int old_elo;
    int new_elo;
    int elo_change;
} EloChange;

int elo_update_ratings(PGconn *db, int winner_id, int loser_id, EloChange *winner_change, EloChange *loser_change);
int elo_save_history(PGconn *db, int user_id, int match_id, int old_elo, int new_elo, int change);

#endif
