#include "protocol_handler.h"
#include "login_handler.h"
#include "game.h"
#include "history.h"
#include "database.h"
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
        
        if (match->status != GAME_PLAYING) {
            char error[] = "ERROR|Game not in playing state\n";
            send(session->socket_fd, error, strlen(error), 0);
            pthread_mutex_unlock(&match->lock);
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
        
        // Update board to final state (after bot move)
        strcpy(match->board.fen, fen_after_bot);
        
        // Save both moves to database
        // Save player move
        history_save_move(db, match_id, session->user_id, player_move, fen_after_player);
        
        // Save bot move (user_id = NULL for bot)
        if (strcmp(bot_move, "NONE") != 0) {
            history_save_bot_move(db, match_id, bot_move, fen_after_bot);
        }
        
        // Check game end condition
        // TODO: Implement proper checkmate/stalemate detection
        char status[32] = "IN_GAME";
        
        // Build response: BOT_MOVE_RESULT|fen_after_player|bot_move|fen_after_bot|status
        char response[2048];
        snprintf(response, sizeof(response),
                "BOT_MOVE_RESULT|%s|%s|%s|%s\n",
                fen_after_player, bot_move, fen_after_bot, status);
        
        send(session->socket_fd, response, strlen(response), 0);
        
        printf("[Bot] Move processed: player=%s, bot=%s\n", player_move, bot_move);
        
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
                        
                        // Notify opponent (need to implement opponent lookup)
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
                
                // Create new match (swap colors)
                char create_match_query[512];
                snprintf(create_match_query, sizeof(create_match_query),
                        "INSERT INTO match_game (type, status) VALUES ('pvp', 'waiting') RETURNING match_id");
                
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
                        
                        // Send response with new match ID
                        char response[256];
                        snprintf(response, sizeof(response), "REMATCH_ACCEPTED|%d\n", new_match_id);
                        send(session->socket_fd, response, strlen(response), 0);
                        
                        printf("[Control] Rematch accepted - New match %d created\n", new_match_id);
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
    if (strcmp(command, "LOGIN") == 0 && num_params >= 3) {
        handle_login(session, param1, param2, db);
    }
    else if (strcmp(command, "START_MATCH") == 0 && num_params >= 5) {
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
    // Leaderboard and ELO commands
    else if (strcmp(command, "GET_LEADERBOARD") == 0) {
        int limit = (num_params >= 2) ? atoi(param1) : 20;
        handle_leaderboard_request(db, session->socket_fd, limit);
    }
    else if (strcmp(command, "GET_ELO_HISTORY") == 0 && num_params >= 2) {
        int user_id = atoi(param1);
        handle_elo_history_request(db, session->socket_fd, user_id);
    }
    else {
        char error[128];
        sprintf(error, "ERROR|Unknown command: %s\n", command);
        send(session->socket_fd, error, strlen(error), 0);
        printf("[Protocol] Unknown command: %s\n", command);
    }
}
