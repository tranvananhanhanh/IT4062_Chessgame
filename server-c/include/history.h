#ifndef HISTORY_H
#define HISTORY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include "game.h"

#define MAX_HISTORY_ITEMS 50
#define MAX_MOVES_PER_GAME 200

// Match history item
typedef struct {
    int match_id;
    int opponent_id;
    char opponent_name[64];
    char result[16];        // "win", "loss", "draw"
    char player_color[8];   // "white", "black"
    char start_time[32];
    char end_time[32];
    int move_count;
} MatchHistoryItem;

// Move record for replay
typedef struct {
    int move_id;
    int user_id;
    char notation[16];
    char fen_after[256];
    char created_at[32];
} MoveRecord;

// Player statistics
typedef struct {
    int user_id;
    int total_games;
    int wins;
    int losses;
    int draws;
    float win_rate;
    int current_elo;
} PlayerStats;

// ============ DATABASE CONNECTION ============
PGconn* db_connect();
void db_disconnect(PGconn *conn);
int db_check_connection(PGconn *conn);

// ============ MATCH HISTORY ============
int history_create_match(PGconn *conn, int user1_id, int user2_id, const char *type);
int history_save_move(PGconn *conn, int match_id, int user_id, const char *notation, const char *fen);
int history_update_match_result(PGconn *conn, int match_id, const char *result, int winner_id);
int history_get_user_matches(PGconn *conn, int user_id, MatchHistoryItem *items, int *count);

// ============ REPLAY ============
int replay_get_moves(PGconn *conn, int match_id, MoveRecord *moves, int *count);
int replay_validate_access(PGconn *conn, int match_id, int user_id);

// ============ STATISTICS ============
int stats_get_player_stats(PGconn *conn, int user_id, PlayerStats *stats);
int stats_update_elo(PGconn *conn, int winner_id, int loser_id);

// ============ HANDLERS ============
void handle_history_request(PGconn *conn, int client_fd, int user_id);
void handle_replay_request(PGconn *conn, int client_fd, int match_id, int user_id);
void handle_stats_request(PGconn *conn, int client_fd, int user_id);

// ============ FORMATTING ============
void format_history_response(MatchHistoryItem *items, int count, char *response);
void format_replay_response(MoveRecord *moves, int count, char *response);
void format_stats_response(PlayerStats *stats, char *response);

#endif
