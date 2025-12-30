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
        int decliner_id = atoi(param2);  // người từ chối
        GameMatch *match = game_manager_find_match(&game_manager, match_id);

        if (match != NULL) {
            pthread_mutex_lock(&match->lock);

            if (match->draw_requester_id != 0 && match->draw_requester_id != decliner_id) {

                // Lưu requester_id trước khi reset
                int requester_id = match->draw_requester_id;
                int requester_fd = (match->white_player.user_id == requester_id) 
                                   ? match->white_player.socket_fd 
                                   : match->black_player.socket_fd;

                // ✅ Update DB - Clear draw offer
                char query[512];
                snprintf(query, sizeof(query),
                         "UPDATE match_player SET action_state='normal' "
                         "WHERE match_id=%d AND user_id=%d AND action_state='draw_offer'",
                         match_id, requester_id);
                
                PGresult *res = PQexec(db, query);
                
                if (PQresultStatus(res) == PGRES_COMMAND_OK) {
                    int rows_affected = atoi(PQcmdTuples(res));
                    if (rows_affected > 0) {
                        // ✅ Clear in-memory state
                        match->draw_requester_id = 0;

                        // Notify requester
                        if (requester_fd > 0 && requester_fd != session->socket_fd) {
                            char notify[256];
                            snprintf(notify, sizeof(notify), 
                                     "DRAW_DECLINED_BY_OPPONENT|%d\n", decliner_id);
                            send(requester_fd, notify, strlen(notify), 0);
                        }

                        // Gửi ACK cho người từ chối
                        char response[] = "DRAW_DECLINED\n";
                        send(session->socket_fd, response, strlen(response), 0);

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
    PGresult *check_res = NULL;

    if (num_params >= 3) {
        int match_id = atoi(param1);
        int player_id = atoi(param2);
        GameMatch *match = game_manager_find_match(&game_manager, match_id);
        if (!match) {
            char error[] = "ERROR|Match not found\n";
            send(session->socket_fd, error, strlen(error), 0);
            PQclear(check_res);
            return;
        }


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
                        match->rematch_requester_id = player_id;   // <--- Thêm dòng này

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
    if (num_params < 3) return;

    int old_match_id = atoi(param1);
    int player_id = atoi(param2);

    printf("[Rematch] Processing REMATCH_ACCEPT for match %d by player %d\n",
           old_match_id, player_id);

    // Kiểm tra rematch request từ đối thủ
    char check_query[512];
    snprintf(check_query, sizeof(check_query),
             "SELECT user_id FROM match_player "
             "WHERE match_id=%d AND action_state='rematch_request' AND user_id != %d",
             old_match_id, player_id);

    PGresult *check_res = PQexec(db, check_query);
    if (PQresultStatus(check_res) != PGRES_TUPLES_OK || PQntuples(check_res) == 0) {
        send(session->socket_fd, "ERROR|No rematch request pending\n", 32, 0);
        PQclear(check_res);
        return;
    }

    // Lấy thông tin match cũ
    GameMatch *old_match = game_manager_find_match(&game_manager, old_match_id);
    if (!old_match) {
        send(session->socket_fd, "ERROR|Old match not found\n", 26, 0);
        PQclear(check_res);
        return;
    }

    int white_user_id = old_match->white_player.user_id;
    int black_user_id = old_match->black_player.user_id;
    char white_username[64], black_username[64];
    strcpy(white_username, old_match->white_player.username);
    strcpy(black_username, old_match->black_player.username);

    int old_white_fd = old_match->white_player.socket_fd;
    int old_black_fd = old_match->black_player.socket_fd;

    // Tạo match mới trong DB
    char create_query[512];
    snprintf(create_query, sizeof(create_query),
             "INSERT INTO match_game (type, status, starttime) "
             "VALUES ('pvp', 'playing', NOW()) RETURNING match_id");
    PGresult *new_match_res = PQexec(db, create_query);
    if (PQresultStatus(new_match_res) != PGRES_TUPLES_OK || PQntuples(new_match_res) == 0) {
        send(session->socket_fd, "ERROR|Failed to create new match\n", 33, 0);
        PQclear(check_res);
        return;
    }

    int new_match_id = atoi(PQgetvalue(new_match_res, 0, 0));

    // Insert players với màu giữ nguyên
    char insert_query[512];
    snprintf(insert_query, sizeof(insert_query),
             "INSERT INTO match_player (match_id, user_id, color, action_state) VALUES "
             "(%d, %d, 'white', 'normal'), (%d, %d, 'black', 'normal')",
             new_match_id, white_user_id, new_match_id, black_user_id);
    PQexec(db, insert_query);

    // Clear action_state cũ
    char clear_query[512];
    snprintf(clear_query, sizeof(clear_query),
             "UPDATE match_player SET action_state='normal' "
             "WHERE match_id=%d AND action_state='rematch_request'",
             old_match_id);
    PQexec(db, clear_query);

    // Tạo match mới trong memory và giữ socket cũ
    pthread_mutex_lock(&game_manager.lock);
    if (game_manager.match_count < MAX_MATCHES) {
        GameMatch *new_match = (GameMatch*)malloc(sizeof(GameMatch));
        new_match->match_id = new_match_id;
        new_match->status = GAME_PLAYING;
        new_match->start_time = time(NULL);
        new_match->end_time = 0;
        new_match->result = RESULT_NONE;
        new_match->winner_id = 0;
        new_match->draw_requester_id = 0;
        new_match->rematch_id = 0;

        // Setup players và giữ socket
        new_match->white_player.user_id = white_user_id;
        new_match->white_player.color = COLOR_WHITE;
        new_match->white_player.socket_fd = old_white_fd; // giữ socket
        new_match->white_player.is_online = (old_white_fd > 0) ? 1 : 0;
        strcpy(new_match->white_player.username, white_username);

        new_match->black_player.user_id = black_user_id;
        new_match->black_player.color = COLOR_BLACK;
        new_match->black_player.socket_fd = old_black_fd; // giữ socket
        new_match->black_player.is_online = (old_black_fd > 0) ? 1 : 0;
        strcpy(new_match->black_player.username, black_username);

        chess_board_init(&new_match->board);
        pthread_mutex_init(&new_match->lock, NULL);

        game_manager.matches[game_manager.match_count++] = new_match;
    }
    pthread_mutex_unlock(&game_manager.lock);

    // Gửi REMATCH_START cho 2 client
    char response[256];
    snprintf(response, sizeof(response), "REMATCH_START|%d|%s\n",
            new_match_id,
            (session->socket_fd == old_white_fd) ? "white" : "black");
    send(session->socket_fd, response, strlen(response), 0);

    int opponent_fd = (old_white_fd == session->socket_fd) ? old_black_fd : old_white_fd;
    if (opponent_fd > 0 && opponent_fd != session->socket_fd) {
        snprintf(response, sizeof(response), "REMATCH_START|%d|%s\n",
                new_match_id,
                (opponent_fd == old_white_fd) ? "white" : "black");
        send(opponent_fd, response, strlen(response), 0);
    }

    printf("[Rematch] New match %d started, white=%d, black=%d\n",
           new_match_id, white_user_id, black_user_id);

    PQclear(check_res);
    PQclear(new_match_res);
}


void handle_rematch_decline(ClientSession *session, int num_params, char *param1, 
                            char *param2, PGconn *db) {
    // REMATCH_DECLINE|match_id|player_id
    if (num_params < 3) {
        char error[] = "ERROR|Invalid REMATCH_DECLINE command format\n";
        send(session->socket_fd, error, strlen(error), 0);
        return;
    }

    int match_id = atoi(param1);
    int decliner_id = atoi(param2);
    GameMatch *match = game_manager_find_match(&game_manager, match_id);

    if (!match) {
        char error[] = "ERROR|Match not found\n";
        send(session->socket_fd, error, strlen(error), 0);
        return;
    }

    pthread_mutex_lock(&match->lock);

    if (match->rematch_requester_id != 0 && match->rematch_requester_id != decliner_id) {
        int requester_id = match->rematch_requester_id;
        int requester_fd = (match->white_player.user_id == requester_id) 
                           ? match->white_player.socket_fd 
                           : match->black_player.socket_fd;

        // ✅ Update DB - Clear rematch request
        char query[512];
        snprintf(query, sizeof(query),
                 "UPDATE match_player SET action_state='normal' "
                 "WHERE match_id=%d AND user_id=%d AND action_state='rematch_request'",
                 match_id, requester_id);
        
        PGresult *res = PQexec(db, query);
        if (PQresultStatus(res) == PGRES_COMMAND_OK) {
            int rows_affected = atoi(PQcmdTuples(res));
            if (rows_affected > 0) {
                // ✅ Clear in-memory state
                match->rematch_requester_id = 0;

                // Notify requester
                if (requester_fd > 0 && requester_fd != session->socket_fd) {
                    char notify[256];
                    snprintf(notify, sizeof(notify),
                             "REMATCH_DECLINED_BY_OPPONENT|%d\n", decliner_id);
                    send(requester_fd, notify, strlen(notify), 0);
                }

                // Gửi ACK cho người từ chối
                char response[] = "REMATCH_DECLINED\n";
                send(session->socket_fd, response, strlen(response), 0);

                printf("[Control] Rematch declined in match %d (DB updated)\n", match_id);
            } else {
                char error[] = "ERROR|No rematch request found to decline\n";
                send(session->socket_fd, error, strlen(error), 0);
            }
        } else {
            char error[] = "ERROR|Failed to update rematch status\n";
            send(session->socket_fd, error, strlen(error), 0);
            printf("[Control] Database error declining rematch: %s\n", PQerrorMessage(db));
        }
        PQclear(res);
    } else {
        char error[] = "ERROR|No rematch request pending or you are the requester\n";
        send(session->socket_fd, error, strlen(error), 0);
    }

    pthread_mutex_unlock(&match->lock);
}
