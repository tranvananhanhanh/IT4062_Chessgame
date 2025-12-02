// control.c - Game control handlers (pause, resume, draw, rematch)
#include "control.h"
#include "game.h"
#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

extern GameManager game_manager;

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
                
                // ⚠️ FIX: Create new match with status='waiting' (will change to 'playing' when both JOIN)
                char create_match_query[512];
                snprintf(create_match_query, sizeof(create_match_query),
                        "INSERT INTO match_game (type, status, starttime) VALUES ('pvp', 'waiting', NOW()) RETURNING match_id");
                
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
                        
                        // ✅ Get player info from OLD match before creating new one
                        GameMatch *old_match = game_manager_find_match(&game_manager, match_id);
                        if (old_match != NULL) {
                            // ✅ Store player info (with FRESH socket FDs from current session)
                            int white_user_id = old_match->black_player.user_id;  // Swap: old black → new white
                            int black_user_id = old_match->white_player.user_id;  // Swap: old white → new black
                            char white_username[64], black_username[64];
                            strcpy(white_username, old_match->black_player.username);
                            strcpy(black_username, old_match->white_player.username);
                            
                            // ✅ Store old match's socket FDs before unlocking
                            int old_white_fd = old_match->white_player.socket_fd;
                            int old_black_fd = old_match->black_player.socket_fd;
                            
                            // ✅ Mark old match with rematch_id BEFORE creating new match
                            pthread_mutex_lock(&old_match->lock);
                            old_match->rematch_id = new_match_id;
                            pthread_mutex_unlock(&old_match->lock);
                            
                            printf("[Rematch] Old match %d finished - creating new match %d (swap colors)\n", 
                                   match_id, new_match_id);
                            
                            // ✅ Create new match with status='waiting' initially (will change to 'playing' when both players JOIN)
                            pthread_mutex_lock(&game_manager.lock);
                            
                            if (game_manager.match_count < MAX_MATCHES) {
                                GameMatch *new_match = (GameMatch*)malloc(sizeof(GameMatch));
                                if (new_match == NULL) {
                                    pthread_mutex_unlock(&game_manager.lock);
                                    char error[] = "ERROR|Failed to allocate memory for new match\n";
                                    send(session->socket_fd, error, strlen(error), 0);
                                    PQclear(insert_res);
                                    PQclear(new_match_res);
                                    PQclear(players_res);
                                    PQclear(check_res);
                                    return;
                                }
                                
                                // Initialize new match
                                new_match->match_id = new_match_id;
                                new_match->status = GAME_WAITING;  // ⚠️ FIX: Start as WAITING, not PLAYING
                                new_match->start_time = time(NULL);
                                new_match->end_time = 0;
                                new_match->result = RESULT_NONE;
                                new_match->winner_id = 0;
                                new_match->draw_requester_id = 0;
                                new_match->rematch_id = 0;
                                
                                // Setup players (NO socket FDs yet - will be set when they JOIN)
                                new_match->white_player.user_id = white_user_id;
                                new_match->white_player.color = COLOR_WHITE;
                                new_match->white_player.socket_fd = -1;  // Will be set on JOIN
                                new_match->white_player.is_online = 0;
                                strcpy(new_match->white_player.username, white_username);
                                
                                new_match->black_player.user_id = black_user_id;
                                new_match->black_player.color = COLOR_BLACK;
                                new_match->black_player.socket_fd = -1;  // Will be set on JOIN
                                new_match->black_player.is_online = 0;
                                strcpy(new_match->black_player.username, black_username);
                                
                                // Initialize board and mutex
                                chess_board_init(&new_match->board);
                                pthread_mutex_init(&new_match->lock, NULL);
                                
                                // Add to manager
                                game_manager.matches[game_manager.match_count] = new_match;
                                game_manager.match_count++;
                                
                                // ⚠️ DON'T create timer yet - will be created when both players JOIN
                                
                                pthread_mutex_unlock(&game_manager.lock);
                                
                                printf("[Rematch] New match %d created (WAITING) - white=%d(%s), black=%d(%s)\n", 
                                       new_match_id, white_user_id, white_username, black_user_id, black_username);
                                
                                // Send response with new match ID
                                char response[256];
                                snprintf(response, sizeof(response), "REMATCH_ACCEPTED|%d\n", new_match_id);
                                send(session->socket_fd, response, strlen(response), 0);
                                
                                // ✅ Notify opponent about new match (use OLD socket FDs)
                                int opponent_fd = (old_white_fd == session->socket_fd) ? old_black_fd : old_white_fd;
                                if (opponent_fd > 0 && opponent_fd != session->socket_fd) {
                                    char notify[256];
                                    snprintf(notify, sizeof(notify), "REMATCH_ACCEPTED|%d\n", new_match_id);
                                    send(opponent_fd, notify, strlen(notify), 0);
                                    printf("[Rematch] Notified opponent (fd=%d) about new match %d\n", 
                                           opponent_fd, new_match_id);
                                }
                                
                                printf("[Rematch] ✅ Both players must call JOIN_MATCH|%d to start playing\n", new_match_id);
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
