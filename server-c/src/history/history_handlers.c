#include "history.h"
#include "game.h"
#include <sys/socket.h>

// Format history response as JSON for Flask backend
void format_history_response(MatchHistoryItem *items, int count, char *response) {
    char json_buffer[BUFFER_SIZE * 4] = "";
    strcpy(json_buffer, "HISTORY|{\"matches\":[");

    if (count == 0) {
        strcat(json_buffer, "]}");
        sprintf(response, "%s\n", json_buffer);
        return;
    }

    for (int i = 0; i < count; i++) {
        char entry[512];
        sprintf(entry, "{\"match_id\":%d,\"opponent_name\":\"%s\",\"result\":\"%s\",\"player_color\":\"%s\",\"start_time\":\"%s\",\"end_time\":\"%s\",\"move_count\":%d}",
                items[i].match_id,
                items[i].opponent_name,
                items[i].result,
                items[i].player_color,
                items[i].start_time,
                items[i].end_time,
                items[i].move_count);

        strcat(json_buffer, entry);
        if (i < count - 1) {
            strcat(json_buffer, ",");
        }
    }

    strcat(json_buffer, "]}");
    sprintf(response, "%s\n", json_buffer);
}

// Format replay response as JSON for Flask backend
void format_replay_response(MoveRecord *moves, int count, char *response) {
    char json_buffer[BUFFER_SIZE * 4] = "";
    sprintf(json_buffer, "REPLAY|{\"move_count\":%d,\"moves\":[", count);

    if (count == 0) {
        strcat(json_buffer, "]}");
        sprintf(response, "%s\n", json_buffer);
        return;
    }

    for (int i = 0; i < count; i++) {
        char entry[512];
        sprintf(entry, "{\"move_number\":%d,\"notation\":\"%s\",\"fen_after\":\"%s\",\"created_at\":\"%s\"}",
                i + 1,
                moves[i].notation,
                moves[i].fen_after,
                moves[i].created_at);

        strcat(json_buffer, entry);
        if (i < count - 1) {
            strcat(json_buffer, ",");
        }
    }

    strcat(json_buffer, "]}");
    sprintf(response, "%s\n", json_buffer);
}

// Format stats response as JSON for Flask backend
void format_stats_response(PlayerStats *stats, char *response) {
    char json_buffer[512];
    sprintf(json_buffer, "STATS|{\"user_id\":%d,\"total_games\":%d,\"wins\":%d,\"losses\":%d,\"draws\":%d,\"win_rate\":%.1f,\"current_elo\":%d}\n",
            stats->user_id,
            stats->total_games,
            stats->wins,
            stats->losses,
            stats->draws,
            stats->win_rate,
            stats->current_elo);
    strcpy(response, json_buffer);
}

// Handle history request
void handle_history_request(PGconn *conn, int client_fd, int user_id) {
    printf("[History] User %d requested match history\n", user_id);
    
    MatchHistoryItem items[MAX_HISTORY_ITEMS];
    int count = 0;
    
    if (!history_get_user_matches(conn, user_id, items, &count)) {
        char error_msg[] = "ERROR|Failed to retrieve history\n";
        send(client_fd, error_msg, strlen(error_msg), 0);
        return;
    }
    
    char response[BUFFER_SIZE * 2];
    format_history_response(items, count, response);
    
    send(client_fd, response, strlen(response), 0);
    
    printf("[History] Sent %d match history items to user %d\n", count, user_id);
}

// Handle replay request
void handle_replay_request(PGconn *conn, int client_fd, int match_id, int user_id) {
    printf("[Replay] User %d requested replay of match %d\n", user_id, match_id);
    
    // Validate access (optional - can allow anyone to view any replay)
    // if (!replay_validate_access(conn, match_id, user_id)) {
    //     char error_msg[] = "ERROR|Access denied to this replay\n";
    //     send(client_fd, error_msg, strlen(error_msg), 0);
    //     return;
    // }
    
    MoveRecord moves[MAX_MOVES_PER_GAME];
    int count = 0;
    
    if (!replay_get_moves(conn, match_id, moves, &count)) {
        char error_msg[] = "ERROR|Failed to retrieve replay\n";
        send(client_fd, error_msg, strlen(error_msg), 0);
        return;
    }
    
    if (count == 0) {
        char error_msg[] = "ERROR|Match not found or has no moves\n";
        send(client_fd, error_msg, strlen(error_msg), 0);
        return;
    }
    
    char response[BUFFER_SIZE * 4];
    format_replay_response(moves, count, response);
    
    send(client_fd, response, strlen(response), 0);
    
    printf("[Replay] Sent %d moves to user %d for match %d\n", count, user_id, match_id);
}

// Handle stats request
void handle_stats_request(PGconn *conn, int client_fd, int user_id) {
    printf("[Stats] User %d requested statistics\n", user_id);
    
    PlayerStats stats;
    
    if (!stats_get_player_stats(conn, user_id, &stats)) {
        char error_msg[] = "ERROR|Failed to retrieve statistics\n";
        send(client_fd, error_msg, strlen(error_msg), 0);
        return;
    }
    
    char response[512];
    format_stats_response(&stats, response);
    
    send(client_fd, response, strlen(response), 0);
    
    printf("[Stats] Sent statistics to user %d: %d games, %.1f%% win rate\n",
           user_id, stats.total_games, stats.win_rate);
}