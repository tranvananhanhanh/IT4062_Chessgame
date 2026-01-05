#ifndef ELO_DB_H
#define ELO_DB_H

#include <libpq-fe.h>
#include "elo_calc.h"
#include "elo.h"

int elo_get_player_rating(PGconn *db, int user_id);
int elo_get_player_games_count(PGconn *db, int user_id);
int elo_update_ratings(PGconn *db, int winner_id, int loser_id, EloChange *winner_change, EloChange *loser_change);
int elo_update_draw(PGconn *db, int player1_id, int player2_id, EloChange *p1_change, EloChange *p2_change);

#endif
