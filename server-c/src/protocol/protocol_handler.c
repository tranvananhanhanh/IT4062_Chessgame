#include "protocol_handler.h"
#include "game.h"
#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// External global variable
extern GameManager game_manager;

// ==================== COMMAND HANDLERS ====================

void handle_start_match(ClientSession *session, char *param1, char *param2, 
                       char *param3, char *param4, PGconn *db) {
    // START_MATCH|user1_id|username1|user2_id|username2
    int user1_id = atoi(param1);
    char username1[64];
    strcpy(username1, param2);
    int user2_id = atoi(param3);
    char username2[64];
    strcpy(username2, param4);
    
    printf("[Protocol] Creating match: %s (ID:%d) vs %s (ID:%d)\n", 
           username1, user1_id, username2, user2_id);
    
    // When called from Flask API, both players use same connection
    Player white = {user1_id, session->socket_fd, COLOR_WHITE, 1, ""};
    strcpy(white.username, username1);
    
    Player black = {user2_id, session->socket_fd, COLOR_BLACK, 1, ""};
    strcpy(black.username, username2);
    
    GameMatch *match = game_manager_create_match(&game_manager, white, black, db);
    
    if (match != NULL) {
        session->current_match = match;
        session->user_id = user1_id;
        strcpy(session->username, username1);
        
        char response[512];
        sprintf(response, "MATCH_CREATED|%d|%s\n", match->match_id, match->board.fen);
        send(session->socket_fd, response, strlen(response), 0);
        
        printf("[Protocol] Match %d created successfully\n", match->match_id);
    } else {
        char error[] = "ERROR|Failed to create match\n";
        send(session->socket_fd, error, strlen(error), 0);
        printf("[Protocol] Failed to create match\n");
    }
}

void handle_join_match(ClientSession *session, char *param1, char *param2, 
                      char *param3, PGconn *db) {
    // JOIN_MATCH|match_id|user_id|username
    (void)db; // Unused for now - may be needed for future validation
    
    int match_id = atoi(param1);
    int user_id = atoi(param2);
    char username[64];
    strcpy(username, param3);
    
    GameMatch *match = game_manager_find_match(&game_manager, match_id);
    
    if (match != NULL) {
        pthread_mutex_lock(&match->lock);
        
        // Assign as black player
        match->black_player.socket_fd = session->socket_fd;
        match->black_player.is_online = 1;
        match->black_player.user_id = user_id;
        strcpy(match->black_player.username, username);
        
        session->current_match = match;
        session->user_id = user_id;
        strcpy(session->username, username);
        
        pthread_mutex_unlock(&match->lock);
        
        char response[300];
        sprintf(response, "MATCH_JOINED|%d|%s\n", match_id, match->board.fen);
        send(session->socket_fd, response, strlen(response), 0);
        
        // Notify white player (if different socket)
        if (match->white_player.socket_fd != session->socket_fd) {
            char notify[256];
            sprintf(notify, "OPPONENT_JOINED|%s\n", username);
            send(match->white_player.socket_fd, notify, strlen(notify), 0);
        }
        
        printf("[Protocol] User %d joined match %d\n", user_id, match_id);
    } else {
        char error[] = "ERROR|Match not found\n";
        send(session->socket_fd, error, strlen(error), 0);
    }
}

void handle_move(ClientSession *session, int num_params, char *param1, 
                char *param2, char *param3, char *param4, PGconn *db) {
    // MOVE|match_id|player_id|from|to (API format)
    // or MOVE|from|to (CLI format)
    
    if (num_params >= 5) {
        // API format: MOVE|match_id|player_id|from|to
        int match_id = atoi(param1);
        int player_id = atoi(param2);
        char *from_square = param3;
        char *to_square = param4;

        GameMatch *match = game_manager_find_match(&game_manager, match_id);
        if (match != NULL) {
            // Find player socket
            int player_socket = -1;
            if (match->white_player.user_id == player_id) {
                player_socket = match->white_player.socket_fd;
            } else if (match->black_player.user_id == player_id) {
                player_socket = match->black_player.socket_fd;
            }

            if (player_socket != -1) {
                game_match_make_move(match, player_socket, player_id, from_square, to_square, db);
            } else {
                char error[] = "ERROR|Player not in match\n";
                send(session->socket_fd, error, strlen(error), 0);
            }
        } else {
            char error[] = "ERROR|Match not found\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
    } else if (num_params >= 3 && session->current_match != NULL) {
        // CLI format: MOVE|from|to
        game_match_make_move(session->current_match, session->socket_fd, 0, param1, param2, db);
    } else {
        char error[] = "ERROR|Invalid MOVE command format\n";
        send(session->socket_fd, error, strlen(error), 0);
    }
}

void handle_surrender(ClientSession *session, int num_params, char *param1, 
                     char *param2, PGconn *db) {
    // SURRENDER|match_id|player_id (API format)
    // or SURRENDER (CLI format)
    
    if (num_params >= 3) {
        // API format: SURRENDER|match_id|player_id
        int match_id = atoi(param1);
        int player_id = atoi(param2);

        GameMatch *match = game_manager_find_match(&game_manager, match_id);
        if (match != NULL) {
            // Find player socket
            int player_socket = -1;
            if (match->white_player.user_id == player_id) {
                player_socket = match->white_player.socket_fd;
            } else if (match->black_player.user_id == player_id) {
                player_socket = match->black_player.socket_fd;
            }

            if (player_socket != -1) {
                game_match_handle_surrender(match, player_socket, db);
            } else {
                char error[] = "ERROR|Player not in match\n";
                send(session->socket_fd, error, strlen(error), 0);
            }
        } else {
            char error[] = "ERROR|Match not found\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
    } else if (session->current_match != NULL) {
        // CLI format: SURRENDER
        game_match_handle_surrender(session->current_match, session->socket_fd, db);
    } else {
        char error[] = "ERROR|Not in a match\n";
        send(session->socket_fd, error, strlen(error), 0);
    }
}

void handle_get_history(ClientSession *session, char *param1, PGconn *db) {
    // GET_HISTORY|user_id
    int user_id = atoi(param1);
    handle_history_request(db, session->socket_fd, user_id);
}

void handle_get_replay(ClientSession *session, char *param1, PGconn *db) {
    // GET_REPLAY|match_id
    int match_id = atoi(param1);
    // Note: Original has user_id param, but API doesn't use it
    handle_replay_request(db, session->socket_fd, match_id, session->user_id);
}

void handle_get_stats(ClientSession *session, char *param1, PGconn *db) {
    // GET_STATS|user_id
    int user_id = atoi(param1);
    handle_stats_request(db, session->socket_fd, user_id);
}

// ==================== PROTOCOL PARSER ====================

void protocol_handle_command(ClientSession *session, const char *buffer, PGconn *db) {
    // Parse command
    char command[32], param1[64], param2[64], param3[64], param4[64];
    memset(command, 0, sizeof(command));
    memset(param1, 0, sizeof(param1));
    memset(param2, 0, sizeof(param2));
    memset(param3, 0, sizeof(param3));
    memset(param4, 0, sizeof(param4));
    
    int num_params = sscanf(buffer, "%[^|]|%[^|]|%[^|]|%[^|]|%s", 
                            command, param1, param2, param3, param4);
    
    printf("[Protocol] Command: '%s' | Params: %d\n", command, num_params);
    
    // Route to appropriate handler
    if (strcmp(command, "START_MATCH") == 0 && num_params >= 5) {
        handle_start_match(session, param1, param2, param3, param4, db);
    }
    else if (strcmp(command, "JOIN_MATCH") == 0 && num_params >= 4) {
        handle_join_match(session, param1, param2, param3, db);
    }
    else if (strcmp(command, "MOVE") == 0) {
        handle_move(session, num_params, param1, param2, param3, param4, db);
    }
    else if (strcmp(command, "SURRENDER") == 0) {
        handle_surrender(session, num_params, param1, param2, db);
    }
    else if (strcmp(command, "GET_HISTORY") == 0 && num_params >= 2) {
        handle_get_history(session, param1, db);
    }
    else if (strcmp(command, "GET_REPLAY") == 0 && num_params >= 2) {
        handle_get_replay(session, param1, db);
    }
    else if (strcmp(command, "GET_STATS") == 0 && num_params >= 2) {
        handle_get_stats(session, param1, db);
    }
    else {
        char error[128];
        sprintf(error, "ERROR|Unknown command: %s\n", command);
        send(session->socket_fd, error, strlen(error), 0);
        printf("[Protocol] Unknown command: %s\n", command);
    }
}
