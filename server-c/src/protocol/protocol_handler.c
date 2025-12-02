#include "protocol_handler.h"
#include "game.h"
#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
    
    // When called from client, assign white to the creator's socket; leave black socket empty for join
    Player white = {user1_id, session->socket_fd, COLOR_WHITE, 1, ""};
    strcpy(white.username, username1);
    
    // Black placeholder: user_id known but socket not assigned yet
    Player black = {user2_id, -1, COLOR_BLACK, 0, ""};
    strcpy(black.username, username2);
    
    GameMatch *match = game_manager_create_match(&game_manager, white, black, db);
    
    if (match != NULL) {
        session->current_match = match;
        session->user_id = user1_id;
        strcpy(session->username, username1);
        
        char response[512];
        sprintf(response, "MATCH_CREATED|%d|%s\n", match->match_id, match->board.fen);
        send(session->socket_fd, response, strlen(response), 0);
        
        printf("[Protocol] Match %d created successfully (waiting for opponent)\n", match->match_id);
    } else {
        char error[] = "ERROR|Failed to create match\n";
        send(session->socket_fd, error, strlen(error), 0);
        printf("[Protocol] Failed to create match\n");
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
    
    if (match != NULL) {
        pthread_mutex_lock(&match->lock);
        
        int is_black_joining = 0;
        
        // ✅ Check which player this user is (white or black)
        if (match->white_player.user_id == user_id) {
            // Joining as white player
            match->white_player.socket_fd = session->socket_fd;
            match->white_player.is_online = 1;
            strcpy(match->white_player.username, username);
            
            printf("[Protocol] User %d joined match %d as WHITE\n", user_id, match_id);
        } else if (match->black_player.user_id == user_id) {
            // Joining as black player
            match->black_player.socket_fd = session->socket_fd;
            match->black_player.is_online = 1;
            strcpy(match->black_player.username, username);
            
            is_black_joining = 1;
            printf("[Protocol] User %d joined match %d as BLACK\n", user_id, match_id);
        } else {
            // User not in this match
            pthread_mutex_unlock(&match->lock);
            char error[] = "ERROR|You are not a player in this match\n";
            send(session->socket_fd, error, strlen(error), 0);
            return;
        }
        
        // ✅ If black player joins AND match is in WAITING state, update to PLAYING
        if (is_black_joining && match->status == GAME_WAITING) {
            // Update database
            char update_query[512];
            snprintf(update_query, sizeof(update_query),
                    "UPDATE match_game SET status='playing' WHERE match_id=%d AND status='waiting'",
                    match_id);
            
            PGresult *res = PQexec(db, update_query);
            if (PQresultStatus(res) == PGRES_COMMAND_OK) {
                // Update in-memory state
                match->status = GAME_PLAYING;
                printf("[Protocol] Match %d status updated: WAITING -> PLAYING (black player joined)\n", match_id);
            } else {
                printf("[Protocol] Warning: Failed to update match %d status in DB: %s\n", 
                       match_id, PQerrorMessage(db));
            }
            PQclear(res);
        }
        
        session->current_match = match;
        session->user_id = user_id;
        strcpy(session->username, username);
        
        pthread_mutex_unlock(&match->lock);
        
        char response[300];
        sprintf(response, "MATCH_JOINED|%d|%s\n", match_id, match->board.fen);
        send(session->socket_fd, response, strlen(response), 0);
        
        // Notify opponent (if different socket and online)
        int opponent_fd = -1;
        if (match->white_player.user_id == user_id) {
            opponent_fd = match->black_player.socket_fd;
        } else {
            opponent_fd = match->white_player.socket_fd;
        }
        
        if (opponent_fd > 0 && opponent_fd != session->socket_fd) {
            char notify[256];
            sprintf(notify, "OPPONENT_JOINED|%s\n", username);
            send(opponent_fd, notify, strlen(notify), 0);
            printf("[Protocol] Notified opponent (fd=%d) that %s joined\n", opponent_fd, username);
        }
        
        printf("[Protocol] User %d joined match %d successfully\n", user_id, match_id);
    } else {
        char error[] = "ERROR|Match not found\n";
        send(session->socket_fd, error, strlen(error), 0);
    }
}

void handle_get_match_status(ClientSession *session, char *param1, PGconn *db) {
    // GET_MATCH_STATUS|match_id
    (void)db; // Unused for now
    
    int match_id = atoi(param1);
    GameMatch *match = game_manager_find_match(&game_manager, match_id);
    
    if (match != NULL) {
        pthread_mutex_lock(&match->lock);
        
        // Format: MATCH_STATUS|match_id|status|white_online|black_online|fen|rematch_id
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
                match_id,
                status_str,
                match->white_player.is_online,
                match->black_player.is_online,
                match->board.fen,
                match->rematch_id);  // ✅ Include rematch_id
        
        pthread_mutex_unlock(&match->lock);
        
        send(session->socket_fd, response, strlen(response), 0);
        printf("[Protocol] Sent match status for match %d: %s, rematch_id=%d\n", 
               match_id, status_str, match->rematch_id);
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
            pthread_mutex_lock(&match->lock);
            
            // ✅ CHECK 1: Game must be in PLAYING state
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
                printf("[Move] Rejected: match %d is not in playing state (status=%d)\n", match_id, match->status);
                return;
            }
            
            // ✅ CHECK 2: Player must be in this match
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
            
            // ✅ CHECK 3: Must be player's turn
            if (match->board.current_turn != player_color) {
                char error[] = "ERROR|Not your turn\n";
                send(session->socket_fd, error, strlen(error), 0);
                pthread_mutex_unlock(&match->lock);
                printf("[Move] Rejected: not player %d's turn in match %d (current_turn=%d)\n", 
                       player_id, match_id, match->board.current_turn);
                return;
            }
            
            pthread_mutex_unlock(&match->lock);
            
            // All checks passed, make the move
            game_match_make_move(match, player_socket, player_id, from_square, to_square, db);
            
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

// ==================== FRIENDSHIP HANDLERS ====================

void handle_friend_request(ClientSession *session, int num_params, char *param1, char *param2, PGconn *db) {
    // FRIEND_REQUEST|user_id|friend_id
    if (num_params >= 3) {
        int user_id = atoi(param1);
        int friend_id = atoi(param2);
        if (user_id == friend_id) {
            char error[] = "ERROR|Cannot add yourself as friend\n";
            send(session->socket_fd, error, strlen(error), 0);
            return;
        }
        // Check if already exists
        char check_query[256];
        snprintf(check_query, sizeof(check_query),
            "SELECT status FROM friendship WHERE user_id=%d AND friend_id=%d;",
            user_id, friend_id);
        PGresult *check_res = PQexec(db, check_query);
        if (PQresultStatus(check_res) == PGRES_TUPLES_OK && PQntuples(check_res) > 0) {
            char error[] = "ERROR|Friend request already exists\n";
            send(session->socket_fd, error, strlen(error), 0);
            PQclear(check_res);
            return;
        }
        PQclear(check_res);
        // Insert new request
        char insert_query[256];
        snprintf(insert_query, sizeof(insert_query),
            "INSERT INTO friendship (user_id, friend_id, status) VALUES (%d, %d, 'pending');",
            user_id, friend_id);
        PGresult *insert_res = PQexec(db, insert_query);
        if (PQresultStatus(insert_res) == PGRES_COMMAND_OK) {
            char response[] = "FRIEND_REQUESTED\n";
            send(session->socket_fd, response, strlen(response), 0);
        } else {
            char error[] = "ERROR|Failed to send friend request\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        PQclear(insert_res);
    }
}

void handle_friend_accept(ClientSession *session, int num_params, char *param1, char *param2, PGconn *db) {
    // FRIEND_ACCEPT|user_id|friend_id
    if (num_params >= 3) {
        int user_id = atoi(param1);
        int friend_id = atoi(param2);
        // Update status
        char update_query[256];
        snprintf(update_query, sizeof(update_query),
            "UPDATE friendship SET status='accepted', responded_at=NOW() WHERE user_id=%d AND friend_id=%d AND status='pending';",
            friend_id, user_id);
        PGresult *update_res = PQexec(db, update_query);
        if (PQresultStatus(update_res) == PGRES_COMMAND_OK && atoi(PQcmdTuples(update_res)) > 0) {
            // Insert reverse relation for easy query
            char insert_query[256];
            snprintf(insert_query, sizeof(insert_query),
                "INSERT INTO friendship (user_id, friend_id, status) VALUES (%d, %d, 'accepted') ON CONFLICT DO NOTHING;",
                user_id, friend_id);
            PQexec(db, insert_query);
            char response[] = "FRIEND_ACCEPTED\n";
            send(session->socket_fd, response, strlen(response), 0);
        } else {
            char error[] = "ERROR|No pending request found\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        PQclear(update_res);
    }
}

void handle_friend_decline(ClientSession *session, int num_params, char *param1, char *param2, PGconn *db) {
    // FRIEND_DECLINE|user_id|friend_id
    if (num_params >= 3) {
        int user_id = atoi(param1);
        int friend_id = atoi(param2);
        char update_query[256];
        snprintf(update_query, sizeof(update_query),
            "UPDATE friendship SET status='declined', responded_at=NOW() WHERE user_id=%d AND friend_id=%d AND status='pending';",
            friend_id, user_id);
        PGresult *update_res = PQexec(db, update_query);
        if (PQresultStatus(update_res) == PGRES_COMMAND_OK && atoi(PQcmdTuples(update_res)) > 0) {
            char response[] = "FRIEND_DECLINED\n";
            send(session->socket_fd, response, strlen(response), 0);
        } else {
            char error[] = "ERROR|No pending request found\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        PQclear(update_res);
    }
}

void handle_friend_list(ClientSession *session, int num_params, char *param1, PGconn *db) {
    // FRIEND_LIST|user_id
    if (num_params >= 2) {
        int user_id = atoi(param1);
        char query[256];
        snprintf(query, sizeof(query),
            "SELECT friend_id FROM friendship WHERE user_id=%d AND status='accepted';",
            user_id);
        PGresult *res = PQexec(db, query);
        if (PQresultStatus(res) == PGRES_TUPLES_OK) {
            char response[1024] = "FRIEND_LIST|";
            for (int i = 0; i < PQntuples(res); i++) {
                strcat(response, PQgetvalue(res, i, 0));
                if (i < PQntuples(res) - 1) strcat(response, ",");
            }
            strcat(response, "\n");
            send(session->socket_fd, response, strlen(response), 0);
        } else {
            char error[] = "ERROR|Failed to get friend list\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        PQclear(res);
    }
}

void handle_friend_requests(ClientSession *session, int num_params, char *param1, PGconn *db) {
    // FRIEND_REQUESTS|user_id
    if (num_params >= 2) {
        int user_id = atoi(param1);
        char query[256];
        snprintf(query, sizeof(query),
            "SELECT user_id FROM friendship WHERE friend_id=%d AND status='pending';",
            user_id);
        PGresult *res = PQexec(db, query);
        if (PQresultStatus(res) == PGRES_TUPLES_OK) {
            char response[1024] = "FRIEND_REQUESTS|";
            for (int i = 0; i < PQntuples(res); i++) {
                strcat(response, PQgetvalue(res, i, 0));
                if (i < PQntuples(res) - 1) strcat(response, ",");
            }
            strcat(response, "\n");
            send(session->socket_fd, response, strlen(response), 0);
        } else {
            char error[] = "ERROR|Failed to get friend requests\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        PQclear(res);
    }
}

// ==================== BOT MODE HANDLERS ====================

void handle_mode_bot(ClientSession *session, char *param1, PGconn *db) {
    // MODE_BOT|user_id
    int user_id = atoi(param1);
    
    printf("[Bot] Initializing bot match for user %d\n", user_id);
    
    // Create a match against bot (black player is bot)
    Player white = {user_id, session->socket_fd, COLOR_WHITE, 1, ""};
    snprintf(white.username, sizeof(white.username), "Player_%d", user_id);
    
    Player black = {0, -1, COLOR_BLACK, 1, "AI_Bot"};  // Bot player with user_id = 0 (will be NULL in DB)
    strcpy(black.username, "AI_Bot");
    
    GameMatch *match = game_manager_create_bot_match(&game_manager, white, black, db);
    
    if (match != NULL) {
        session->current_match = match;
        session->user_id = user_id;
        strcpy(session->username, white.username);
        
        char response[512];
        sprintf(response, "BOT_MATCH_CREATED|%d|%s\n", match->match_id, match->board.fen);
        send(session->socket_fd, response, strlen(response), 0);
        
        printf("[Bot] Bot match %d created successfully\n", match->match_id);
    } else {
        char error[] = "ERROR|Failed to create bot match\n";
        send(session->socket_fd, error, strlen(error), 0);
        printf("[Bot] Failed to create bot match\n");
    }
}

// Helper function to communicate with Python chess bot
char* call_python_bot(const char *fen, const char *player_move, char *bot_move_out, size_t bot_move_size) {
    // Connect to Python bot server (default port 5001)
    const char *bot_host = getenv("BOT_HOST") ? getenv("BOT_HOST") : "127.0.0.1";
    int bot_port = getenv("BOT_PORT") ? atoi(getenv("BOT_PORT")) : 5001;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[Bot] Socket creation failed");
        return NULL;
    }
    
    struct sockaddr_in bot_addr;
    memset(&bot_addr, 0, sizeof(bot_addr));
    bot_addr.sin_family = AF_INET;
    bot_addr.sin_port = htons(bot_port);
    
    if (inet_pton(AF_INET, bot_host, &bot_addr.sin_addr) <= 0) {
        perror("[Bot] Invalid address");
        close(sock);
        return NULL;
    }
    
    if (connect(sock, (struct sockaddr *)&bot_addr, sizeof(bot_addr)) < 0) {
        perror("[Bot] Connection to bot server failed");
        close(sock);
        return NULL;
    }
    
    // Send: "fen|player_move\n"
    char request[2048];
    snprintf(request, sizeof(request), "%s|%s\n", fen, player_move);
    
    if (send(sock, request, strlen(request), 0) < 0) {
        perror("[Bot] Send to bot failed");
        close(sock);
        return NULL;
    }
    
    // Receive: "fen_after_player|bot_move|fen_after_bot\n"
    char response[4096];
    ssize_t bytes_read = recv(sock, response, sizeof(response) - 1, 0);
    close(sock);
    
    if (bytes_read <= 0) {
        fprintf(stderr, "[Bot] No response from bot server\n");
        return NULL;
    }
    
    response[bytes_read] = '\0';
    
    // Remove trailing newline
    char *newline = strchr(response, '\n');
    if (newline) *newline = '\0';
    
    printf("[Bot] Received from Python bot: %s\n", response);
    
    // Parse "fen_after_player|bot_move|fen_after_bot"
    char *p1 = strchr(response, '|');
    if (!p1) {
        fprintf(stderr, "[Bot] Invalid response format (missing first |)\n");
        return NULL;
    }
    *p1 = '\0';
    char *fen_after_player = response;
    
    char *p2 = strchr(p1 + 1, '|');
    if (!p2) {
        fprintf(stderr, "[Bot] Invalid response format (missing second |)\n");
        return NULL;
    }
    *p2 = '\0';
    char *bot_move = p1 + 1;
    char *fen_after_bot = p2 + 1;
    
    // Copy bot move to output buffer
    strncpy(bot_move_out, bot_move, bot_move_size);
    bot_move_out[bot_move_size - 1] = '\0';
    
    // Return combined FEN string "fen_after_player|fen_after_bot"
    char *result = (char*)malloc(strlen(fen_after_player) + strlen(fen_after_bot) + 2);
    sprintf(result, "%s|%s", fen_after_player, fen_after_bot);
    
    return result;
}

void handle_bot_move(ClientSession *session, int num_params, char *param1, 
                    char *param2, char *param3, PGconn *db) {
    // BOT_MOVE|match_id|player_move (API format)
    // or BOT_MOVE|player_move (CLI format)
    
    if (num_params >= 3) {
        // API format: BOT_MOVE|match_id|player_move
        int match_id = atoi(param1);
        char *player_move = param2;
        
        GameMatch *match = game_manager_find_match(&game_manager, match_id);
        if (match == NULL) {
            char error[] = "ERROR|Match not found\n";
            send(session->socket_fd, error, strlen(error), 0);
            return;
        }
        
        pthread_mutex_lock(&match->lock);
        
        // ✅ CHECK 1: Game must be in PLAYING state
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
            printf("[Bot Move] Rejected: match %d status=%d\n", match_id, match->status);
            return;
        }
        
        // ✅ CHECK 2: Must be white's turn (player's turn)
        if (match->board.current_turn != COLOR_WHITE) {
            char error[] = "ERROR|Not your turn (bot is thinking)\n";
            send(session->socket_fd, error, strlen(error), 0);
            pthread_mutex_unlock(&match->lock);
            printf("[Bot Move] Rejected: not white's turn in match %d\n", match_id);
            return;
        }
        
        // Get current FEN
        char current_fen[FEN_MAX_LENGTH];
        strcpy(current_fen, match->board.fen);
        
        printf("[Bot] Processing move %s on FEN: %s\n", player_move, current_fen);
        
        // Call Python bot to get FENs and bot move
        char bot_move[16] = {0};
        char *fen_combo = call_python_bot(current_fen, player_move, bot_move, sizeof(bot_move));
        
        if (fen_combo == NULL) {
            char error[] = "ERROR|Bot engine failed\n";
            send(session->socket_fd, error, strlen(error), 0);
            pthread_mutex_unlock(&match->lock);
            return;
        }
        
        // Parse "fen_after_player|fen_after_bot"
        char *sep = strchr(fen_combo, '|');
        if (!sep) {
            char error[] = "ERROR|Invalid bot response format\n";
            send(session->socket_fd, error, strlen(error), 0);
            free(fen_combo);
            pthread_mutex_unlock(&match->lock);
            return;
        }
        *sep = '\0';
        char *fen_after_player = fen_combo;
        char *fen_after_bot = sep + 1;
        
        // ✅ EXECUTE PLAYER MOVE ON BOARD (to update board state, not just FEN string)
        Move player_move_struct;
        notation_to_coords(player_move, &player_move_struct.from_row, &player_move_struct.from_col);
        notation_to_coords(player_move + 2, &player_move_struct.to_row, &player_move_struct.to_col);
        player_move_struct.piece = match->board.board[player_move_struct.from_row][player_move_struct.from_col];
        player_move_struct.captured_piece = match->board.board[player_move_struct.to_row][player_move_struct.to_col];
        player_move_struct.is_castling = 0;
        player_move_struct.is_en_passant = 0;
        player_move_struct.is_promotion = 0;
        player_move_struct.promotion_piece = 0;
        execute_move(&match->board, &player_move_struct);
        
        // ✅ EXECUTE BOT MOVE ON BOARD (to update board state)
        // Special case: if bot returns NOMOVE, it means bot has no legal moves (checkmate or stalemate)
        if (strcmp(bot_move, "NOMOVE") != 0 && strcmp(bot_move, "NONE") != 0) {
            Move bot_move_struct;
            notation_to_coords(bot_move, &bot_move_struct.from_row, &bot_move_struct.from_col);
            notation_to_coords(bot_move + 2, &bot_move_struct.to_row, &bot_move_struct.to_col);
            bot_move_struct.piece = match->board.board[bot_move_struct.from_row][bot_move_struct.from_col];
            bot_move_struct.captured_piece = match->board.board[bot_move_struct.to_row][bot_move_struct.to_col];
            bot_move_struct.is_castling = 0;
            bot_move_struct.is_en_passant = 0;
            bot_move_struct.is_promotion = 0;
            bot_move_struct.promotion_piece = 0;
            execute_move(&match->board, &bot_move_struct);
            
            // Update FEN string to final state after bot move
            strcpy(match->board.fen, fen_after_bot);
        } else {
            // Bot has no moves - update FEN to after player move only
            strcpy(match->board.fen, fen_after_player);
            printf("[Bot] Bot returned NOMOVE - checking if checkmate or stalemate\n");
            
            // ✅ Manually check if this is checkmate or stalemate
            if (is_king_in_check(&match->board, COLOR_BLACK)) {
                // Black is in check and has no moves = CHECKMATE
                printf("[Bot] Black is in checkmate! White wins!\n");
                match->status = GAME_FINISHED;
                match->result = RESULT_WHITE_WIN;
                match->winner_id = match->white_player.user_id;
                match->end_time = time(NULL);
                
                history_update_match_result(db, match->match_id, "white_win", match->winner_id);
                timer_manager_stop_timer(&game_manager.timer_manager, match->match_id);
                
                char msg[BUFFER_SIZE];
                sprintf(msg, "GAME_END|checkmate|winner:%d\n", match->winner_id);
                broadcast_to_match(match, msg, -1);
                
                // For bot match: send BOT_GAME_END
                sprintf(msg, "BOT_GAME_END|white_win|winner:%d\n", match->white_player.user_id);
                send_to_client(match->white_player.socket_fd, msg);
                
            } else {
                // Black is NOT in check but has no moves = STALEMATE
                printf("[Bot] Stalemate! Draw!\n");
                match->status = GAME_FINISHED;
                match->result = RESULT_DRAW;
                match->end_time = time(NULL);
                
                history_update_match_result(db, match->match_id, "draw", 0);
                timer_manager_stop_timer(&game_manager.timer_manager, match->match_id);
                
                char msg[BUFFER_SIZE];
                sprintf(msg, "GAME_END|stalemate|draw\n");
                broadcast_to_match(match, msg, -1);
                
                // For bot match: send BOT_GAME_END
                sprintf(msg, "BOT_GAME_END|draw\n");
                send_to_client(match->white_player.socket_fd, msg);
            }
        }
        
        // Save both moves to database
        history_save_move(db, match_id, session->user_id, player_move, fen_after_player);
        if (strcmp(bot_move, "NONE") != 0) {
            history_save_bot_move(db, match_id, bot_move, fen_after_bot);
        }
        
        // ✅ CHECK GAME END CONDITION AFTER BOT MOVE (now board state is updated!)
        game_match_check_end_condition(match, db);
        
        // Determine game status for response
        char status[32] = "IN_GAME";
        printf("[DEBUG] After game_match_check_end_condition: match->status=%d (0=WAITING,1=PLAYING,2=FINISHED)\n", match->status);
        printf("[DEBUG] match->result=%d (0=NONE,1=WHITE_WIN,2=BLACK_WIN,3=DRAW)\n", match->result);
        
        if (match->status == GAME_FINISHED) {
            if (match->result == RESULT_WHITE_WIN) {
                strcpy(status, "WHITE_WIN");
            } else if (match->result == RESULT_BLACK_WIN) {
                strcpy(status, "BLACK_WIN");
            } else if (match->result == RESULT_DRAW) {
                strcpy(status, "DRAW");
            }
        }
        
        printf("[DEBUG] Status string being sent: '%s'\n", status);
        
        // Build response: BOT_MOVE_RESULT|fen_after_player|bot_move|fen_after_bot|status
        char response[2048];
        snprintf(response, sizeof(response),
                "BOT_MOVE_RESULT|%s|%s|%s|%s\n",
                fen_after_player, bot_move, fen_after_bot, status);
        
        send(session->socket_fd, response, strlen(response), 0);
        
        printf("[Bot] Move processed: player=%s, bot=%s, status=%s\n", 
               player_move, bot_move, status);
        
        free(fen_combo);
        pthread_mutex_unlock(&match->lock);
        
    } else if (num_params >= 2 && session->current_match != NULL) {
        // CLI format: BOT_MOVE|player_move
        // Use session's current match
        char *player_move = param1;
        
        pthread_mutex_lock(&session->current_match->lock);
        
        char current_fen[FEN_MAX_LENGTH];
        strcpy(current_fen, session->current_match->board.fen);
        
        char bot_move[16] = {0};
        char *fen_combo = call_python_bot(current_fen, player_move, bot_move, sizeof(bot_move));
        
        if (fen_combo) {
            char *sep = strchr(fen_combo, '|');
            if (sep) {
                *sep = '\0';
                strcpy(session->current_match->board.fen, sep + 1);
                
                // ✅ CHECK GAME END CONDITION AFTER BOT MOVE
                game_match_check_end_condition(session->current_match, db);
                
                char response[512];
                snprintf(response, sizeof(response),
                        "BOT_MOVE_RESULT|%s|%s\n", bot_move, sep + 1);
                send(session->socket_fd, response, strlen(response), 0);
            }
            free(fen_combo);
        } else {
            char error[] = "ERROR|Bot engine failed\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        
        pthread_mutex_unlock(&session->current_match->lock);
    } else {
        char error[] = "ERROR|Invalid BOT_MOVE command format\n";
        send(session->socket_fd, error, strlen(error), 0);
    }
}

// ==================== GAME CONTROL HANDLERS ====================

void handle_pause(ClientSession *session, int num_params, char *param1, 
                 char *param2, PGconn *db) {
    if (num_params >= 3) {
        int match_id = atoi(param1);
        int player_id = atoi(param2);
        
        GameMatch *match = game_manager_find_match(&game_manager, match_id);
        if (match != NULL) {
            pthread_mutex_lock(&match->lock);
            
            if (match->status == GAME_PLAYING) {
                // ✅ UPDATE DATABASE - Set status to 'paused'
                char query[512];
                snprintf(query, sizeof(query),
                        "UPDATE match_game SET status='paused' WHERE match_id=%d AND status='playing'",
                        match_id);
                
                PGresult *res = PQexec(db, query);
                if (PQresultStatus(res) == PGRES_COMMAND_OK) {
                    // ✅ Update in-memory state  
                    match->status = GAME_PAUSED;  // Need to add GAME_PAUSED to enum
                    
                    char response[] = "GAME_PAUSED\n";
                    send(session->socket_fd, response, strlen(response), 0);
                    
                    // Notify opponent
                    int opponent_fd = (match->white_player.user_id == player_id) 
                                      ? match->black_player.socket_fd 
                                      : match->white_player.socket_fd;
                    if (opponent_fd != session->socket_fd && opponent_fd > 0) {
                        char notify[] = "GAME_PAUSED_BY_OPPONENT\n";
                        send(opponent_fd, notify, strlen(notify), 0);
                    }
                    
                    printf("[Control] Match %d paused by player %d (DB updated)\n", match_id, player_id);
                } else {
                    char error[] = "ERROR|Failed to pause game\n";
                    send(session->socket_fd, error, strlen(error), 0);
                    printf("[Control] Database error pausing match %d: %s\n", match_id, PQerrorMessage(db));
                }
                PQclear(res);
            } else {
                char error[] = "ERROR|Game is not in playing state\n";
                send(session->socket_fd, error, strlen(error), 0);
            }
            
            pthread_mutex_unlock(&match->lock);
        } else {
            char error[] = "ERROR|Match not found\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
    }
}

void handle_resume(ClientSession *session, int num_params, char *param1, 
                  char *param2, PGconn *db) {
    if (num_params >= 3) {
        int match_id = atoi(param1);
        int player_id = atoi(param2);
        
        GameMatch *match = game_manager_find_match(&game_manager, match_id);
        if (match != NULL) {
            pthread_mutex_lock(&match->lock);
            
            // ✅ Check if game is actually paused in database
            char check_query[256];
            snprintf(check_query, sizeof(check_query),
                    "SELECT status FROM match_game WHERE match_id=%d", match_id);
            
            PGresult *check_res = PQexec(db, check_query);
            if (PQresultStatus(check_res) == PGRES_TUPLES_OK && PQntuples(check_res) > 0) {
                char *current_status = PQgetvalue(check_res, 0, 0);
                
                if (strcmp(current_status, "paused") == 0) {
                    // ✅ UPDATE DATABASE - Resume game (back to 'playing')
                    char update_query[512];
                    snprintf(update_query, sizeof(update_query),
                            "UPDATE match_game SET status='playing' WHERE match_id=%d",
                            match_id);
                    
                    PGresult *res = PQexec(db, update_query);
                    if (PQresultStatus(res) == PGRES_COMMAND_OK) {
                        // ✅ Update in-memory state
                        match->status = GAME_PLAYING;
                        
                        char response[] = "GAME_RESUMED\n";
                        send(session->socket_fd, response, strlen(response), 0);
                        
                        // Notify opponent
                        int opponent_fd = (match->white_player.user_id == player_id) 
                                          ? match->black_player.socket_fd 
                                          : match->white_player.socket_fd;
                        if (opponent_fd != session->socket_fd && opponent_fd > 0) {
                            char notify[] = "GAME_RESUMED\n";
                            send(opponent_fd, notify, strlen(notify), 0);
                        }
                        
                        printf("[Control] Match %d resumed by player %d (DB updated)\n", match_id, player_id);
                    } else {
                        char error[] = "ERROR|Failed to resume game\n";
                        send(session->socket_fd, error, strlen(error), 0);
                        printf("[Control] Database error resuming match %d: %s\n", match_id, PQerrorMessage(db));
                    }
                    PQclear(res);
                } else {
                    char error[100];
                    snprintf(error, sizeof(error), "ERROR|Game is not paused (status: %s)\n", current_status);
                    send(session->socket_fd, error, strlen(error), 0);
                }
            } else {
                char error[] = "ERROR|Failed to check game status\n";
                send(session->socket_fd, error, strlen(error), 0);
            }
            PQclear(check_res);
            
            pthread_mutex_unlock(&match->lock);
        } else {
            char error[] = "ERROR|Match not found\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
    }
}

void handle_draw_request(ClientSession *session, int num_params, char *param1, 
                        char *param2, PGconn *db) {
    // DRAW|match_id|player_id (API format)
    
    if (num_params >= 3) {
        int match_id = atoi(param1);
        int player_id = atoi(param2);
        
        GameMatch *match = game_manager_find_match(&game_manager, match_id);
        if (match != NULL) {
            pthread_mutex_lock(&match->lock);
            
            // ✅ Check if game is playing and no pending draw request
            if (match->status == GAME_PLAYING && match->draw_requester_id == 0) {
                
                // ✅ UPDATE DATABASE - Set match_player.action_state = 'draw_offer'
                char query[512];
                snprintf(query, sizeof(query),
                        "UPDATE match_player SET action_state='draw_offer' "
                        "WHERE match_id=%d AND user_id=%d AND action_state='normal'",
                        match_id, player_id);
                
                PGresult *res = PQexec(db, query);
                
                if (PQresultStatus(res) == PGRES_COMMAND_OK) {
                    int rows_affected = atoi(PQcmdTuples(res));
                    
                    if (rows_affected > 0) {
                        // ✅ Update in-memory state
                        match->draw_requester_id = player_id;
                        
                        char response[] = "DRAW_REQUESTED\n";
                        send(session->socket_fd, response, strlen(response), 0);
                        
                        // Notify opponent
                        int opponent_fd = (match->white_player.user_id == player_id) 
                                          ? match->black_player.socket_fd 
                                          : match->white_player.socket_fd;
                        if (opponent_fd != session->socket_fd && opponent_fd > 0) {
                            char notify[256];
                            snprintf(notify, sizeof(notify), 
                                    "DRAW_REQUEST_FROM_OPPONENT|%d\n", player_id);
                            send(opponent_fd, notify, strlen(notify), 0);
                        }
                        
                        printf("[Control] Player %d requested draw in match %d (DB updated)\n", 
                               player_id, match_id);
                    } else {
                        char error[] = "ERROR|Player not found in match or already has pending action\n";
                        send(session->socket_fd, error, strlen(error), 0);
                    }
                } else {
                    char error[] = "ERROR|Failed to record draw request\n";
                    send(session->socket_fd, error, strlen(error), 0);
                    printf("[Control] Database error recording draw request: %s\n", 
                           PQerrorMessage(db));
                }
                PQclear(res);
                
            } else if (match->status != GAME_PLAYING) {
                char error[] = "ERROR|Game is not in playing state\n";
                send(session->socket_fd, error, strlen(error), 0);
            } else {
                char error[] = "ERROR|Draw request already pending\n";
                send(session->socket_fd, error, strlen(error), 0);
            }
            
            pthread_mutex_unlock(&match->lock);
        } else {
            char error[] = "ERROR|Match not found\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
    } else {
        char error[] = "ERROR|Invalid DRAW command format\n";
        send(session->socket_fd, error, strlen(error), 0);
    }
}

void handle_draw_accept(ClientSession *session, int num_params, char *param1, 
                       char *param2, PGconn *db) {
    // DRAW_ACCEPT|match_id|player_id
    
    if (num_params >= 3) {
        int match_id = atoi(param1);
        int player_id = atoi(param2);
        
        GameMatch *match = game_manager_find_match(&game_manager, match_id);
        if (match != NULL) {
            pthread_mutex_lock(&match->lock);
            
            if (match->draw_requester_id != 0 && 
                match->draw_requester_id != player_id) {
                
                // ✅ Clear all draw offers before ending game
                char clear_query[512];
                snprintf(clear_query, sizeof(clear_query),
                        "UPDATE match_player SET action_state='normal' "
                        "WHERE match_id=%d AND action_state='draw_offer'",
                        match_id);
                
                PGresult *clear_res = PQexec(db, clear_query);
                
                if (PQresultStatus(clear_res) == PGRES_COMMAND_OK) {
                    // ✅ End game with draw result
                    match->status = GAME_FINISHED;
                    match->result = RESULT_DRAW;
                    match->end_time = time(NULL);
                    
                    // Update match_game
                    char query[512];
                    snprintf(query, sizeof(query),
                            "UPDATE match_game SET status='finished', result='draw', endtime=CURRENT_TIMESTAMP "
                            "WHERE match_id=%d",
                            match_id);
                    
                    PGresult *res = PQexec(db, query);
                    
                    if (PQresultStatus(res) == PGRES_COMMAND_OK) {
                        char response[] = "DRAW_ACCEPTED\n";
                        send(session->socket_fd, response, strlen(response), 0);
                        
                        // Notify opponent
                        int opponent_fd = (match->white_player.user_id == player_id) 
                                          ? match->black_player.socket_fd 
                                          : match->white_player.socket_fd;
                        if (opponent_fd != session->socket_fd && opponent_fd > 0) {
                            char notify[] = "DRAW_ACCEPTED\n";
                            send(opponent_fd, notify, strlen(notify), 0);
                        }
                        
                        printf("[Control] Draw accepted in match %d - Game ended (DB updated)\n", 
                               match_id);
                    } else {
                        char error[] = "ERROR|Failed to end game\n";
                        send(session->socket_fd, error, strlen(error), 0);
                    }
                    PQclear(res);
                } else {
                    char error[] = "ERROR|Failed to clear draw requests\n";
                    send(session->socket_fd, error, strlen(error), 0);
                }
                PQclear(clear_res);
                
            } else {
                char error[] = "ERROR|No draw request pending or you are the requester\n";
                send(session->socket_fd, error, strlen(error), 0);
            }
            
            pthread_mutex_unlock(&match->lock);
        } else {
            char error[] = "ERROR|Match not found\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
    } else {
        char error[] = "ERROR|Invalid DRAW_ACCEPT command format\n";
        send(session->socket_fd, error, strlen(error), 0);
    }
}

void handle_draw_decline(ClientSession *session, int num_params, char *param1, 
                        char *param2, PGconn *db) {
    // DRAW_DECLINE|match_id|player_id
    
    if (num_params >= 3) {
        int match_id = atoi(param1);
        int player_id = atoi(param2);
        
        GameMatch *match = game_manager_find_match(&game_manager, match_id);
        if (match != NULL) {
            pthread_mutex_lock(&match->lock);
            
            if (match->draw_requester_id != 0 && 
                match->draw_requester_id != player_id) {
                
                // ✅ UPDATE DATABASE - Clear draw offer (back to 'normal')
                char query[512];
                snprintf(query, sizeof(query),
                        "UPDATE match_player SET action_state='normal' "
                        "WHERE match_id=%d AND user_id=%d AND action_state='draw_offer'",
                        match_id, match->draw_requester_id);
                
                PGresult *res = PQexec(db, query);
                
                if (PQresultStatus(res) == PGRES_COMMAND_OK) {
                    int rows_affected = atoi(PQcmdTuples(res));
                    
                    if (rows_affected > 0) {
                        // ✅ Clear in-memory state
                        match->draw_requester_id = 0;
                        
                        char response[] = "DRAW_DECLINED\n";
                        send(session->socket_fd, response, strlen(response), 0);
                        
                        // Notify requester
                        int requester_fd = (match->white_player.user_id == match->draw_requester_id) 
                                          ? match->white_player.socket_fd 
                                          : match->black_player.socket_fd;
                        if (requester_fd != session->socket_fd && requester_fd > 0) {
                            char notify[256];
                            snprintf(notify, sizeof(notify), 
                                    "DRAW_DECLINED_BY_OPPONENT|%d\n", player_id);
                            send(requester_fd, notify, strlen(notify), 0);
                        }
                        
                        printf("[Control] Draw declined in match %d (DB updated)\n", match_id);
                    } else {
                        char error[] = "ERROR|No draw offer found to decline\n";
                        send(session->socket_fd, error, strlen(error), 0);
                    }
                } else {
                    char error[] = "ERROR|Failed to update draw status\n";
                    send(session->socket_fd, error, strlen(error), 0);
                    printf("[Control] Database error declining draw: %s\n", 
                           PQerrorMessage(db));
                }
                PQclear(res);
                
            } else {
                char error[] = "ERROR|No draw request pending or you are the requester\n";
                send(session->socket_fd, error, strlen(error), 0);
            }
            
            pthread_mutex_unlock(&match->lock);
        } else {
            char error[] = "ERROR|Match not found\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
    } else {
        char error[] = "ERROR|Invalid DRAW_DECLINE command format\n";
        send(session->socket_fd, error, strlen(error), 0);
    }
}

void handle_rematch_request(ClientSession *session, int num_params, char *param1, 
                           char *param2, PGconn *db) {
    // REMATCH|match_id|player_id
    
    if (num_params >= 3) {
        int match_id = atoi(param1);
        int player_id = atoi(param2);
        
        // ✅ Check if match is finished
        char check_query[512];
        snprintf(check_query, sizeof(check_query),
                "SELECT status FROM match_game WHERE match_id=%d", match_id);
        
        PGresult *check_res = PQexec(db, check_query);
        
        if (PQresultStatus(check_res) == PGRES_TUPLES_OK && PQntuples(check_res) > 0) {
            char *game_status = PQgetvalue(check_res, 0, 0);
            
            if (strcmp(game_status, "finished") == 0) {
                // ✅ UPDATE DATABASE - Set match_player.action_state = 'rematch_request'
                char query[512];
                snprintf(query, sizeof(query),
                        "UPDATE match_player SET action_state='rematch_request' "
                        "WHERE match_id=%d AND user_id=%d AND action_state='normal'",
                        match_id, player_id);
                
                PGresult *res = PQexec(db, query);
                
                if (PQresultStatus(res) == PGRES_COMMAND_OK) {
                    int rows_affected = atoi(PQcmdTuples(res));
                    
                    if (rows_affected > 0) {
                        char response[] = "REMATCH_REQUESTED\n";
                        send(session->socket_fd, response, strlen(response), 0);
                        
                        // ✅ Notify opponent about rematch request
                        GameMatch *match = game_manager_find_match(&game_manager, match_id);
                        if (match != NULL) {
                            int opponent_fd = -1;
                            if (match->white_player.user_id == player_id) {
                                opponent_fd = match->black_player.socket_fd;
                            } else if (match->black_player.user_id == player_id) {
                                opponent_fd = match->white_player.socket_fd;
                            }
                            
                            if (opponent_fd > 0 && opponent_fd != session->socket_fd) {
                                char notify[] = "OPPONENT_REMATCH_REQUEST\n";
                                send(opponent_fd, notify, strlen(notify), 0);
                            }
                        }
                        
                        printf("[Control] Player %d requested rematch in match %d\n", 
                               player_id, match_id);
                    } else {
                        char error[] = "ERROR|Player not found or already has pending action\n";
                        send(session->socket_fd, error, strlen(error), 0);
                    }
                } else {
                    char error[] = "ERROR|Failed to record rematch request\n";
                    send(session->socket_fd, error, strlen(error), 0);
                }
                PQclear(res);
            } else {
                char error[] = "ERROR|Game is not finished\n";
                send(session->socket_fd, error, strlen(error), 0);
            }
        } else {
            char error[] = "ERROR|Match not found\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        PQclear(check_res);
    }
}

void handle_rematch_accept(ClientSession *session, int num_params, char *param1, 
                          char *param2, PGconn *db) {
    // REMATCH_ACCEPT|match_id|player_id
    
    if (num_params >= 3) {
        int match_id = atoi(param1);
        int player_id = atoi(param2);
        
        printf("[Rematch] Processing REMATCH_ACCEPT for match %d by player %d\n", match_id, player_id);
        
        // ✅ Check if there's a pending rematch request (NOT from this player)
        char check_query[512];
        snprintf(check_query, sizeof(check_query),
                "SELECT user_id FROM match_player "
                "WHERE match_id=%d AND action_state='rematch_request' AND user_id != %d",
                match_id, player_id);
        
        PGresult *check_res = PQexec(db, check_query);
        
        if (PQresultStatus(check_res) == PGRES_TUPLES_OK && PQntuples(check_res) > 0) {
            int requester_id = atoi(PQgetvalue(check_res, 0, 0));
            
            // ✅ Create new match with same players
            char get_players_query[512];
            snprintf(get_players_query, sizeof(get_players_query),
                    "SELECT user_id, color FROM match_player WHERE match_id=%d ORDER BY color",
                    match_id);
            
            PGresult *players_res = PQexec(db, get_players_query);
            
            if (PQresultStatus(players_res) == PGRES_TUPLES_OK && PQntuples(players_res) == 2) {
                int player1_id = atoi(PQgetvalue(players_res, 0, 0)); // white
                int player2_id = atoi(PQgetvalue(players_res, 1, 0)); // black
                
                // Create new match (swap colors) with status='playing'
                char create_match_query[512];
                snprintf(create_match_query, sizeof(create_match_query),
                        "INSERT INTO match_game (type, status, starttime) VALUES ('pvp', 'playing', NOW()) RETURNING match_id");
                
                PGresult *new_match_res = PQexec(db, create_match_query);
                
                if (PQresultStatus(new_match_res) == PGRES_TUPLES_OK && PQntuples(new_match_res) > 0) {
                    int new_match_id = atoi(PQgetvalue(new_match_res, 0, 0));
                    
                    // Insert players with swapped colors
                    char insert_players_query[512];
                    snprintf(insert_players_query, sizeof(insert_players_query),
                            "INSERT INTO match_player (match_id, user_id, color, action_state) VALUES "
                            "(%d, %d, 'white', 'normal'), (%d, %d, 'black', 'normal')",
                            new_match_id, player2_id, new_match_id, player1_id);
                    
                    PGresult *insert_res = PQexec(db, insert_players_query);
                    
                    if (PQresultStatus(insert_res) == PGRES_COMMAND_OK) {
                        // Clear old match rematch requests
                        char clear_query[512];
                        snprintf(clear_query, sizeof(clear_query),
                                "UPDATE match_player SET action_state='normal' "
                                "WHERE match_id=%d AND action_state='rematch_request'",
                                match_id);
                        PQexec(db, clear_query);
                        
                        // ✅ Create GameMatch object in memory with swapped colors (WITHOUT creating DB record)
                        GameMatch *old_match = game_manager_find_match(&game_manager, match_id);
                        if (old_match != NULL) {
                            // Swap player info and colors
                            Player new_white = old_match->black_player;
                            new_white.color = COLOR_WHITE;
                            new_white.socket_fd = -1;  // Will be set when players JOIN
                            new_white.is_online = 0;
                            
                            Player new_black = old_match->white_player;
                            new_black.color = COLOR_BLACK;
                            new_black.socket_fd = -1;  // Will be set when players JOIN
                            new_black.is_online = 0;
                            
                            // ✅ Manually allocate and initialize match (don't call game_manager_create_match - it creates DB record)
                            pthread_mutex_lock(&game_manager.lock);
                            
                            if (game_manager.match_count < MAX_MATCHES) {
                                GameMatch *new_match = (GameMatch*)malloc(sizeof(GameMatch));
                                new_match->match_id = new_match_id;  // Use ID from database
                                new_match->status = GAME_PLAYING;
                                new_match->white_player = new_white;
                                new_match->black_player = new_black;
                                new_match->start_time = time(NULL);
                                new_match->end_time = 0;
                                new_match->result = RESULT_NONE;
                                new_match->winner_id = 0;
                                new_match->draw_requester_id = 0;
                                new_match->rematch_id = 0;
                                
                                chess_board_init(&new_match->board);
                                pthread_mutex_init(&new_match->lock, NULL);
                                
                                // Add to manager
                                game_manager.matches[game_manager.match_count] = new_match;
                                game_manager.match_count++;
                                
                                // Create timer
                                timer_manager_create_timer(&game_manager.timer_manager, new_match_id, 30, game_timeout_callback);
                                
                                pthread_mutex_unlock(&game_manager.lock);
                                
                                // ✅ Store rematch_id in old match so polling can detect it
                                pthread_mutex_lock(&old_match->lock);
                                old_match->rematch_id = new_match_id;
                                pthread_mutex_unlock(&old_match->lock);
                                
                                printf("[Control] New match %d created in memory (white=%d, black=%d)\n", 
                                       new_match_id, new_white.user_id, new_black.user_id);
                                printf("[Control] Stored rematch_id=%d in old match %d\n", 
                                       new_match_id, match_id);
                                
                                // Send response with new match ID to acceptor
                                char response[256];
                                snprintf(response, sizeof(response), "REMATCH_ACCEPTED|%d\n", new_match_id);
                                send(session->socket_fd, response, strlen(response), 0);
                                
                                // ✅ Notify opponent (requester) about new match
                                int opponent_fd = -1;
                                if (old_match->white_player.user_id == player_id) {
                                    opponent_fd = old_match->black_player.socket_fd;
                                } else if (old_match->black_player.user_id == player_id) {
                                    opponent_fd = old_match->white_player.socket_fd;
                                }
                                
                                if (opponent_fd > 0 && opponent_fd != session->socket_fd) {
                                    char notify[256];
                                    snprintf(notify, sizeof(notify), "REMATCH_ACCEPTED|%d\n", new_match_id);
                                    send(opponent_fd, notify, strlen(notify), 0);
                                    printf("[Control] Notified opponent (fd=%d) about new match %d\n", 
                                           opponent_fd, new_match_id);
                                }
                            } else {
                                pthread_mutex_unlock(&game_manager.lock);
                                char error[] = "ERROR|Maximum matches reached\n";
                                send(session->socket_fd, error, strlen(error), 0);
                            }
                        } else {
                            char error[] = "ERROR|Old match not found in memory\n";
                            send(session->socket_fd, error, strlen(error), 0);
                        }
                    }
                    PQclear(insert_res);
                }
                PQclear(new_match_res);
            }
            PQclear(players_res);
        } else {
            char error[] = "ERROR|No rematch request pending\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        PQclear(check_res);
    }
}

void handle_rematch_decline(ClientSession *session, int num_params, char *param1, 
                           char *param2, PGconn *db) {
    // REMATCH_DECLINE|match_id|player_id
    
    if (num_params >= 3) {
        int match_id = atoi(param1);
        int player_id = atoi(param2);
        
        // ✅ Clear rematch request
        char query[512];
        snprintf(query, sizeof(query),
                "UPDATE match_player SET action_state='normal' "
                "WHERE match_id=%d AND action_state='rematch_request'",
                match_id);
        
        PGresult *res = PQexec(db, query);
        
        if (PQresultStatus(res) == PGRES_COMMAND_OK) {
            char response[] = "REMATCH_DECLINED\n";
            send(session->socket_fd, response, strlen(response), 0);
            
            printf("[Control] Rematch declined in match %d\n", match_id);
        } else {
            char error[] = "ERROR|Failed to decline rematch\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        PQclear(res);
    }
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
    else {
        char error[128];
        sprintf(error, "ERROR|Unknown command: %s\n", command);
        send(session->socket_fd, error, strlen(error), 0);
        printf("[Protocol] Unknown command: %s\n", command);
    }
}
