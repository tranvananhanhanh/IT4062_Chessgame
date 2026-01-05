#include "match.h"
#include "game.h"
#include "elo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// External global variable
extern GameManager game_manager;

void handle_start_match(ClientSession *session, char *param1, char *param2, 
                       char *param3, char *param4, PGconn *db) {
    // START_MATCH|user1_id|username1|user2_id|username2
    int user1_id = atoi(param1);
    char username1[64];
    strcpy(username1, param2);
    int user2_id = atoi(param3);
    char username2[64];
    strcpy(username2, param4);
    
    printf("[Match] Creating match: %s (ID:%d) vs %s (ID:%d)\n", 
           username1, user1_id, username2, user2_id);
    
    // White player (creator) - connected now
    Player white = {user1_id, session->socket_fd, COLOR_WHITE, 1, ""};
    strcpy(white.username, username1);
    
    // Black player (placeholder) - will join later
    Player black = {user2_id, -1, COLOR_BLACK, 0, ""};
    strcpy(black.username, username2);
    
    GameMatch *match = game_manager_create_match(&game_manager, white, black, db);
    if (match != NULL) {
        // Force match to PLAYING state immediately so both players can play right after creation
        match->status = GAME_PLAYING;
        // Optionally, update DB as well (if needed for consistency)
        char update_query[128];
        snprintf(update_query, sizeof(update_query),
            "UPDATE match_game SET status='playing' WHERE match_id=%d", match->match_id);
        PGresult *res = PQexec(db, update_query);
        PQclear(res);
        
        session->current_match = match;
        session->user_id = user1_id;
        strcpy(session->username, username1);
        
        char response[512];
        sprintf(response, "MATCH_CREATED|%d|%s\n", match->match_id, match->board.fen);
        send(session->socket_fd, response, strlen(response), 0);
        
        // Send initial timer
        char timer_msg[BUFFER_SIZE];
        snprintf(timer_msg, sizeof(timer_msg),
                 "TIMER_UPDATE|%d|%d\n",
                 match->white_time_remaining,
                 match->black_time_remaining);
        send(session->socket_fd, timer_msg, strlen(timer_msg), 0);
        
        printf("[Match] Match %d created successfully (waiting for opponent)\n", match->match_id);
    } else {
        char error[] = "ERROR|Failed to create match\n";
        send(session->socket_fd, error, strlen(error), 0);
        printf("[Match] Failed to create match\n");
    }
}

void handle_join_match(ClientSession *session, char *param1, char *param2, 
                      char *param3, PGconn *db) {
    // JOIN_MATCH|match_id|user_id|username
    
    int match_id = atoi(param1);
    int user_id = atoi(param2);
    char username[64];
    strcpy(username, param3);
    
    GameMatch *match = game_manager_find_match(&game_manager, match_id);
    
    if (match == NULL) {
        char error[] = "ERROR|Match not found\n";
        send(session->socket_fd, error, strlen(error), 0);
        return;
    }
    
    pthread_mutex_lock(&match->lock);
    
    int is_black_joining = 0;
    
    // Check which player this user is (white or black)
    if (match->white_player.user_id == user_id) {
        match->white_player.socket_fd = session->socket_fd;
        match->white_player.is_online = 1;
        strcpy(match->white_player.username, username);
        printf("[Match] User %d joined match %d as WHITE\n", user_id, match_id);
    } else if (match->black_player.user_id == user_id) {
        match->black_player.socket_fd = session->socket_fd;
        match->black_player.is_online = 1;
        strcpy(match->black_player.username, username);
        is_black_joining = 1;
        printf("[Match] User %d joined match %d as BLACK\n", user_id, match_id);
    } else {
        pthread_mutex_unlock(&match->lock);
        char error[] = "ERROR|You are not a player in this match\n";
        send(session->socket_fd, error, strlen(error), 0);
        return;
    }
    
    // ✅ FIX: If black player joins AND match is in WAITING state, update to PLAYING
    if (is_black_joining && match->status == GAME_WAITING) {
        char update_query[512];
        snprintf(update_query, sizeof(update_query),
                "UPDATE match_game SET status='playing' WHERE match_id=%d AND status='waiting'",
                match_id);
        
        PGresult *res = PQexec(db, update_query);
        if (PQresultStatus(res) == PGRES_COMMAND_OK) {
            match->status = GAME_PLAYING;
            
            // ✅ FIX: Create timer when match starts (for rematch scenario)
            timer_manager_create_timer(&game_manager.timer_manager, match_id, 30, game_timeout_callback);
            
            printf("[Match] ✅ Match %d: WAITING → PLAYING (timer created)\n", match_id);
        } else {
            printf("[Match] ⚠️ Failed to update match %d status: %s\n", 
                   match_id, PQerrorMessage(db));
        }
        PQclear(res);
    } else if (!is_black_joining && match->status == GAME_WAITING) {
        // White player joined first - match still waiting for black
        printf("[Match] White player joined - waiting for black player to start\n");
    }
    
    session->current_match = match;
    session->user_id = user_id;
    strcpy(session->username, username);
    
    pthread_mutex_unlock(&match->lock);
    
    char response[300];
    sprintf(response, "MATCH_JOINED|%d|%s\n", match_id, match->board.fen);
    send(session->socket_fd, response, strlen(response), 0);
    
    // Send initial timer
    char timer_msg[BUFFER_SIZE];
    snprintf(timer_msg, sizeof(timer_msg),
             "TIMER_UPDATE|%d|%d\n",
             match->white_time_remaining,
             match->black_time_remaining);
    send(session->socket_fd, timer_msg, strlen(timer_msg), 0);
    
    // Notify opponent
    int opponent_fd = (match->white_player.user_id == user_id) 
                      ? match->black_player.socket_fd 
                      : match->white_player.socket_fd;
    
    if (opponent_fd > 0 && opponent_fd != session->socket_fd) {
        char notify[256];
        sprintf(notify, "OPPONENT_JOINED|%s\n", username);
        send(opponent_fd, notify, strlen(notify), 0);
        printf("[Match] Notified opponent (fd=%d) that %s joined\n", opponent_fd, username);
    }
    
    printf("[Match] User %d joined match %d successfully\n", user_id, match_id);
}

void handle_get_match_status(ClientSession *session, char *param1, PGconn *db) {
    // GET_MATCH_STATUS|match_id
    (void)db;
    
    int match_id = atoi(param1);
    GameMatch *match = game_manager_find_match(&game_manager, match_id);
    
    if (match == NULL) {
        char error[] = "ERROR|Match not found\n";
        send(session->socket_fd, error, strlen(error), 0);
        return;
    }
    
    pthread_mutex_lock(&match->lock);
    
    const char *status_str;
    switch (match->status) {
        case GAME_WAITING: status_str = "WAITING"; break;
        case GAME_PLAYING: status_str = "PLAYING"; break;
        case GAME_PAUSED: status_str = "PAUSED"; break;
        case GAME_FINISHED: status_str = "FINISHED"; break;
        default: status_str = "UNKNOWN"; break;
    }
    
    char response[512];
    snprintf(response, sizeof(response), 
            "MATCH_STATUS|%d|%s|%d|%d|%s|%d\n",
            match_id, status_str,
            match->white_player.is_online,
            match->black_player.is_online,
            match->board.fen,
            match->rematch_id);
    
    pthread_mutex_unlock(&match->lock);
    
    send(session->socket_fd, response, strlen(response), 0);
    printf("[Match] Sent status for match %d: %s, rematch_id=%d\n", 
           match_id, status_str, match->rematch_id);
}

void handle_move(ClientSession *session, int num_params, char *param1, 
                char *param2, char *param3, char *param4, PGconn *db) {
    // MOVE|match_id|player_id|from|to (API) or MOVE|from|to (CLI)
    
    if (num_params >= 5) {
        // API format
        int match_id = atoi(param1);
        int player_id = atoi(param2);
        char *from_square = param3;
        char *to_square = param4;

        GameMatch *match = game_manager_find_match(&game_manager, match_id);
        if (match == NULL) {
            char error[] = "ERROR|Match not found\n";
            send(session->socket_fd, error, strlen(error), 0);
            return;
        }
        
        pthread_mutex_lock(&match->lock);
        
        // CHECK 1: Game must be PLAYING
        if (match->status != GAME_PLAYING) {
            char error[100];
            if (match->status == GAME_FINISHED) {
                snprintf(error, sizeof(error), "ERROR|Game has ended\n");
            } else if (match->status == GAME_PAUSED) {
                snprintf(error, sizeof(error), "ERROR|Game is paused\n");
            } else {
                snprintf(error, sizeof(error), "ERROR|Game is not in playing state\n");
            }
            send(session->socket_fd, error, strlen(error), 0);
            pthread_mutex_unlock(&match->lock);
            printf("[Move] Rejected: match %d status=%d\n", match_id, match->status);
            return;
        }
        
        // CHECK 2: Player must be in match
        int player_socket = -1;
        PlayerColor player_color;
        if (match->white_player.user_id == player_id) {
            player_socket = match->white_player.socket_fd;
            player_color = COLOR_WHITE;
        } else if (match->black_player.user_id == player_id) {
            player_socket = match->black_player.socket_fd;
            player_color = COLOR_BLACK;
        } else {
            char error[] = "ERROR|Player not in match\n";
            send(session->socket_fd, error, strlen(error), 0);
            pthread_mutex_unlock(&match->lock);
            printf("[Move] Rejected: player %d not in match %d\n", player_id, match_id);
            return;
        }
        
        // CHECK 3: Must be player's turn
        if (match->board.current_turn != player_color) {
            char error[] = "ERROR|Not your turn\n";
            send(session->socket_fd, error, strlen(error), 0);
            pthread_mutex_unlock(&match->lock);
            printf("[Move] Rejected: not player %d's turn (current=%d)\n", 
                   player_id, match->board.current_turn);
            return;
        }
        
        pthread_mutex_unlock(&match->lock);
        
        // All checks passed
        game_match_make_move(match, player_socket, player_id, from_square, to_square, db);
        
    } else if (num_params >= 3 && session->current_match != NULL) {
        // CLI format
        game_match_make_move(session->current_match, session->socket_fd, 0, param1, param2, db);
    } else {
        char error[] = "ERROR|Invalid MOVE command format\n";
        send(session->socket_fd, error, strlen(error), 0);
    }
}

void handle_surrender(ClientSession *session, int num_params, char *param1, 
                     char *param2, PGconn *db) {
    // SURRENDER|match_id|player_id (API) or SURRENDER (CLI)
    
    if (num_params >= 3) {
        // API format
        int match_id = atoi(param1);
        int player_id = atoi(param2);

        GameMatch *match = game_manager_find_match(&game_manager, match_id);
        if (match == NULL) {
            char error[] = "ERROR|Match not found\n";
            send(session->socket_fd, error, strlen(error), 0);
            return;
        }
        
        int player_socket = -1;
        if (match->white_player.user_id == player_id) {
            player_socket = match->white_player.socket_fd;
        } else if (match->black_player.user_id == player_id) {
            player_socket = match->black_player.socket_fd;
        }

        if (player_socket != -1) {
            // TÍCH HỢP ELO: Nếu surrender, xác định winner/loser và cập nhật ELO
            int winner_id = (match->white_player.user_id == player_id) ? match->black_player.user_id : match->white_player.user_id;
            int loser_id = player_id;
            EloChange winner_change, loser_change;
            if (winner_id > 0 && loser_id > 0) {
                elo_update_ratings(db, winner_id, loser_id, &winner_change, &loser_change);
                // Lưu lịch sử ELO
                int match_id = match->match_id;
                elo_save_history(db, winner_id, match_id, winner_change.old_elo, winner_change.new_elo, winner_change.elo_change);
                elo_save_history(db, loser_id, match_id, loser_change.old_elo, loser_change.new_elo, loser_change.elo_change);
            }
            game_match_handle_surrender(match, player_socket, db);
        } else {
            char error[] = "ERROR|Player not in match\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
    } else if (session->current_match != NULL) {
        // CLI format
        game_match_handle_surrender(session->current_match, session->socket_fd, db);
    } else {
        char error[] = "ERROR|Not in a match\n";
        send(session->socket_fd, error, strlen(error), 0);
    }
}

void handle_create_match(ClientSession *session, char *param1, char *param2, PGconn *db) {
    // TODO: Implement actual logic
    printf("handle_create_match called (stub)\n");
}

void handle_get_game_state(ClientSession *session, char *param1, PGconn *db) {
    // TODO: Implement actual logic
    printf("handle_get_game_state called (stub)\n");
}
