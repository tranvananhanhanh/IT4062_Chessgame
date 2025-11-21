#ifndef DATABASE_H
#define DATABASE_H

#include <libpq-fe.h>

// ==================== CONNECTION ====================
PGconn* db_connect();
void db_disconnect(PGconn *conn);
int db_check_connection(PGconn *conn);

// ==================== MATCHES ====================
int db_create_match(PGconn *conn, int user1_id, int user2_id, const char *type);
int db_update_match_status(PGconn *conn, int match_id, const char *status, 
                           const char *result, int winner_id);
int db_get_match_status(PGconn *conn, int match_id, char *status, int status_size);

// ==================== MOVES ====================
int db_save_move(PGconn *conn, int match_id, int user_id, 
                const char *notation, const char *fen);
int db_get_match_moves(PGconn *conn, int match_id, char *output, int output_size);

// ==================== USERS & STATS ====================
int db_get_user_matches(PGconn *conn, int user_id, char *output, int output_size);
int db_get_user_stats(PGconn *conn, int user_id, char *output, int output_size);
int db_update_user_stats(PGconn *conn, int user_id, const char *result);

#endif // DATABASE_H
