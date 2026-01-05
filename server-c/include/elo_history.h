#ifndef ELO_HISTORY_H
#define ELO_HISTORY_H

#include <libpq-fe.h>

int elo_save_history(PGconn *db, int user_id, int match_id, int old_elo, int new_elo, int change);
int elo_get_history(PGconn *db, int user_id, char *output, int output_size);

#endif
