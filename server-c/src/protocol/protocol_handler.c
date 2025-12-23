// protocol_handler.c - Protocol command router
#include "protocol_handler.h"
#include "match.h"
#include "friend.h"
#include "bot.h"
#include "control.h"
#include "history.h"
#include "matchmaking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// External global variable
extern GameManager game_manager;

<<<<<<< HEAD
// ==================== COMMAND HANDLERS ====================

void handle_register_validate(ClientSession *session, int num_params,
                             char *param1, char *param2, char *param3, PGconn *db) {
    (void)db; // not using DB in validation

    // REGISTER_VALIDATE|username|password|email
    if (num_params < 4) {
        const char *msg = "REGISTER_ERROR|Missing username or password or email\n";
        send(session->socket_fd, msg, strlen(msg), 0);
        return;
    }

    const char *username = param1;
    const char *password = param2;
    const char *email = param3;

    // Trim whitespace
    while (*username == ' ' || *username == '\t') username++;
    while (*password == ' ' || *password == '\t') password++;
    while (*email == ' ' || *email == '\t') email++;

    // Validate username length
    size_t ulen = strlen(username);
    if (ulen < 3 || ulen > 50) {
        const char *msg = "REGISTER_ERROR|Username length must be 3-50 characters\n";
        send(session->socket_fd, msg, strlen(msg), 0);
        return;
    }

    // Validate username characters
    for (size_t i = 0; i < ulen; ++i) {
        char c = username[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-')) {
            const char *msg = "REGISTER_ERROR|Username contains invalid characters\n";
            send(session->socket_fd, msg, strlen(msg), 0);
            return;
        }
    }

    // Validate password length
    if (strlen(password) < 6) {
        const char *msg = "REGISTER_ERROR|Password must be at least 6 characters\n";
        send(session->socket_fd, msg, strlen(msg), 0);
        return;
    }

    // Validate email
    if (strlen(email) < 5 || strchr(email, '@') == NULL || strchr(email, '.') == NULL) {
        const char *msg = "REGISTER_ERROR|Invalid email format\n";
        send(session->socket_fd, msg, strlen(msg), 0);
        return;
    }

    // EVERYTHING OK
    const char *ok = "REGISTER_OK\n";
    send(session->socket_fd, ok, strlen(ok), 0);
}

void handle_protocol_line(const char *line, char *out, size_t out_size) {
    // Matchmaking commands
    if (strncmp(line, "MMJOIN", 6) == 0) {
        const char *payload = line + 6;
        while (*payload == ' ') payload++;
        handle_mmjoin(payload, out, out_size);
        return;
    }

    if (strncmp(line, "MMSTATUS", 8) == 0) {
        const char *payload = line + 8;
        while (*payload == ' ') payload++;
        handle_mmstatus(payload, out, out_size);
        return;
    }

    if (strncmp(line, "MMCANCEL", 8) == 0) {
        const char *payload = line + 8;
        while (*payload == ' ') payload++;
        handle_mmcancel(payload, out, out_size);
        return;
    }
}

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

=======
// History handler wrappers
>>>>>>> feature/playgameva
void handle_get_history(ClientSession *session, char *param1, PGconn *db) {
    int user_id = atoi(param1);
    handle_history_request(db, session->socket_fd, user_id);
}

void handle_get_replay(ClientSession *session, char *param1, PGconn *db) {
    int match_id = atoi(param1);
    handle_replay_request(db, session->socket_fd, match_id, session->user_id);
}

void handle_get_stats(ClientSession *session, char *param1, PGconn *db) {
    int user_id = atoi(param1);
    handle_stats_request(db, session->socket_fd, user_id);
}

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
    else if (strcmp(command, "GET_MATCH_STATUS") == 0 && num_params >= 2) {
        handle_get_match_status(session, param1, db);
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
    // Thêm case validate đăng kí
    else if (strcmp(command, "REGISTER_VALIDATE") == 0) {
        handle_register_validate(session, num_params, param1, param2, param3, db);
    }

    // Friendship commands
    else if (strcmp(command, "FRIEND_REQUEST") == 0) {
        handle_friend_request(session, num_params, param1, param2, db);
    }
    else if (strcmp(command, "FRIEND_ACCEPT") == 0) {
        handle_friend_accept(session, num_params, param1, param2, db);
    }
    else if (strcmp(command, "FRIEND_DECLINE") == 0) {
        handle_friend_decline(session, num_params, param1, param2, db);
    }
    else if (strcmp(command, "FRIEND_LIST") == 0) {
        handle_friend_list(session, num_params, param1, db);
    }
    else if (strcmp(command, "FRIEND_REQUESTS") == 0) {
        handle_friend_requests(session, num_params, param1, db);
    }
    // Bot mode commands
    else if (strcmp(command, "MODE_BOT") == 0 && num_params >= 2) {
        handle_mode_bot(session, param1, db);
    }
    else if (strcmp(command, "BOT_MOVE") == 0) {
        handle_bot_move(session, num_params, param1, param2, param3, db);
    }
    // Game control commands
    else if (strcmp(command, "PAUSE") == 0) {
        handle_pause(session, num_params, param1, param2, db);
    }
    else if (strcmp(command, "RESUME") == 0) {
        handle_resume(session, num_params, param1, param2, db);
    }
    else if (strcmp(command, "DRAW") == 0) {
        handle_draw_request(session, num_params, param1, param2, db);
    }
    else if (strcmp(command, "DRAW_ACCEPT") == 0) {
        handle_draw_accept(session, num_params, param1, param2, db);
    }
    else if (strcmp(command, "DRAW_DECLINE") == 0) {
        handle_draw_decline(session, num_params, param1, param2, db);
    }
    else if (strcmp(command, "REMATCH") == 0) {
        handle_rematch_request(session, num_params, param1, param2, db);
    }
    else if (strcmp(command, "REMATCH_ACCEPT") == 0) {
        handle_rematch_accept(session, num_params, param1, param2, db);
    }
    else if (strcmp(command, "REMATCH_DECLINE") == 0) {
        handle_rematch_decline(session, num_params, param1, param2, db);
    }
    
    // === MATCHMAKING COMMANDS ===
    else if (strcmp(command, "MMJOIN") == 0 && num_params >= 4) {
        // param1 = user_id, param2 = elo, param3 = time_mode
        char payload[256];
        snprintf(payload, sizeof(payload), "%s %s %s", param1, param2, param3);

        char resp[256];
        memset(resp, 0, sizeof(resp));

        handle_mmjoin(payload, resp, sizeof(resp));
        send(session->socket_fd, resp, strlen(resp), 0);
    }
    else if (strcmp(command, "MMSTATUS") == 0 && num_params >= 2) {
        // param1 = user_id
        char resp[256];
        memset(resp, 0, sizeof(resp));

        handle_mmstatus(param1, resp, sizeof(resp));
        send(session->socket_fd, resp, strlen(resp), 0);
    }
    else if (strcmp(command, "MMCANCEL") == 0 && num_params >= 2) {
        // param1 = user_id
        char resp[256];
        memset(resp, 0, sizeof(resp));

        handle_mmcancel(param1, resp, sizeof(resp));
        send(session->socket_fd, resp, strlen(resp), 0);
    }
    
    else {
        char error[128];
        sprintf(error, "ERROR|Unknown command: %s\n", command);
        send(session->socket_fd, error, strlen(error), 0);
        printf("[Protocol] Unknown command: %s\n", command);
    }
}
