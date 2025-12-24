#ifndef ELO_LEADERBOARD_H
#define ELO_LEADERBOARD_H

#include <libpq-fe.h>

int elo_get_leaderboard(PGconn *db, char *output, int output_size, int limit);

#endif
