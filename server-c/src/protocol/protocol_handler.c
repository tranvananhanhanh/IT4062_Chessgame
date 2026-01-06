// protocol_handler.c - Protocol command router
#include "protocol_handler.h"
#include "database.h"
#include "login.h"
#include "match.h"
#include "bot.h"
#include "friend.h"
#include "control.h"
#include "history.h"
#include "matchmaking.h"
#include "timer.h"
#include "elo_leaderboard.h"
#include "elo_history.h"
#include "chat.h"
#include "game_chat.h"
#include "email_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// External global variable
extern GameManager game_manager;

// ==================== HELPER FUNCTION ====================

// Parse command string: command|param1|param2|param3|rest_of_message
// This function properly handles messages containing pipes
// Returns: number of parameters found (at least 1 for command)
static int parse_command(const char *buffer, 
                        char *command, size_t cmd_size,
                        char *param1, size_t p1_size,
                        char *param2, size_t p2_size,
                        char *param3, size_t p3_size,
                        char *param4, size_t p4_size) {
    if (!buffer || !command) return 0;
    
    memset(command, 0, cmd_size);
    if (param1) memset(param1, 0, p1_size);
    if (param2) memset(param2, 0, p2_size);
    if (param3) memset(param3, 0, p3_size);
    if (param4) memset(param4, 0, p4_size);
    
    const char *pos = buffer;
    int param_count = 0;
    char *targets[] = {command, param1, param2, param3, param4};
    size_t sizes[] = {cmd_size, p1_size, p2_size, p3_size, p4_size};
    
    for (int i = 0; i < 5; i++) {
        if (!targets[i]) continue;
        
        // Find next pipe or end of string
        const char *pipe = strchr(pos, '|');
        size_t len;
        
        if (pipe) {
            len = pipe - pos;
        } else {
            // Last parameter - take everything to end
            len = strlen(pos);
        }
        
        if (len >= sizes[i]) len = sizes[i] - 1;
        
        strncpy(targets[i], pos, len);
        targets[i][len] = '\0';
        param_count++;
        
        if (!pipe) break;  // No more pipes, we're done
        
        pos = pipe + 1;  // Move past the pipe
    }
    
    return param_count;
}

// ==================== COMMAND HANDLERS ====================

// History handler wrappers
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
    char command[32], param1[256], param2[1024], param3[64], param4[64];
    
    int num_params = parse_command(buffer, command, sizeof(command),
                                   param1, sizeof(param1),
                                   param2, sizeof(param2),
                                   param3, sizeof(param3),
                                   param4, sizeof(param4));
    
    printf("[Protocol] Command: '%s' | Params: %d\n", command, num_params);
    
    // Route to appropriate handler
    if (strcmp(command, "LOGIN") == 0) {
        handle_login(session, param1, param2, db);
    } else if (strcmp(command, "LOGOUT") == 0) {
        handle_logout(session);
    } else if (strcmp(command, "REGISTER") == 0) {
        handle_register_validate(session, num_params, param1, param2, param3, db);
    } else if (strcmp(command, "REGISTER_VALIDATE") == 0) {
        handle_register_validate(session, num_params, param1, param2, param3, db);
    } else if (strcmp(command, "FORGOT_PASSWORD") == 0) {
        // FORGOT_PASSWORD|email
        char email[128] = {0};
        int user_id = db_get_user_email(db, param1, email, sizeof(email));
        
        if (user_id > 0) {
            char otp[7];
            generate_otp(otp, 6);
            
            if (db_save_otp(db, user_id, otp)) {
                if (send_otp_email(email, otp)) {
                    char resp[256];
                    snprintf(resp, sizeof(resp), "OTP_SENT|%d|%s\n", user_id, email);
                    send(session->socket_fd, resp, strlen(resp), 0);
                    printf("[Password Reset] OTP sent to %s for user_id %d\n", email, user_id);
                } else {
                    send(session->socket_fd, "ERROR|Failed to send OTP email\n", 32, 0);
                }
            } else {
                send(session->socket_fd, "ERROR|Failed to save OTP\n", 26, 0);
            }
        } else {
            send(session->socket_fd, "ERROR|User not found or no email registered\n", 45, 0);
        }
    } else if (strcmp(command, "RESET_PASSWORD") == 0) {
        // RESET_PASSWORD|user_id|otp|new_password
        int user_id = atoi(param1);
        
        if (db_reset_password(db, user_id, param3, param2)) {
            send(session->socket_fd, "PASSWORD_RESET_OK\n", 18, 0);
            printf("[Password Reset] Password reset successful for user_id %d\n", user_id);
        } else {
            send(session->socket_fd, "ERROR|Invalid or expired OTP\n", 30, 0);
        }
    } else if (strcmp(command, "CHAT") == 0) {
        // CHAT|to_user|message
        handle_chat(session, num_params, param1, param2);
    } else if (strcmp(command, "GAME_CHAT") == 0) {
        // GAME_CHAT|match_id|message
        int match_id = atoi(param1);
        handle_game_chat(session, match_id, param2, db);
    }
    // Match/game logic
    else if (strcmp(command, "CREATE_MATCH") == 0) {
        handle_create_match(session, param1, param2, db);
    } else if (strcmp(command, "JOIN_MATCH") == 0) {
        handle_join_match(session, param1, param2, param3, db);
    } else if (strcmp(command, "START_MATCH") == 0) {
        handle_start_match(session, param1, param2, param3, param4, db);
    } else if (strcmp(command, "GET_MATCH_STATUS") == 0) {
        handle_get_match_status(session, param1, db);
    } else if (strcmp(command, "MOVE") == 0) {
        handle_move(session, num_params, param1, param2, param3, param4, db);
    } else if (strcmp(command, "SURRENDER") == 0) {
        handle_surrender(session, num_params, param1, param2, db);
    } else if (strcmp(command, "GET_GAME_STATE") == 0) {
        handle_get_game_state(session, param1, db);
    } else if (strcmp(command, "GET_HISTORY") == 0) {
        handle_get_history(session, param1, db);
    } else if (strcmp(command, "GET_REPLAY") == 0) {
        handle_get_replay(session, param1, db);
    } else if (strcmp(command, "GET_STATS") == 0) {
        handle_get_stats(session, param1, db);
    }
    // Bot
    else if (strcmp(command, "MODE_BOT") == 0) {
        // MODE_BOT|user_id|difficulty
        handle_mode_bot(session, param1, param2, db);
    } else if (strcmp(command, "BOT_MOVE") == 0) {
        // BOT_MOVE|match_id|player_move|difficulty
        handle_bot_move(session, num_params, param1, param2, param3, db);
    }
    // Friend
    else if (strcmp(command, "FRIEND_REQUEST") == 0) {
        handle_friend_request(session, num_params, param1, param2, db);
    } else if (strcmp(command, "FRIEND_ACCEPT") == 0) {
        handle_friend_accept(session, num_params, param1, param2, db);
    } else if (strcmp(command, "FRIEND_DECLINE") == 0) {
        handle_friend_decline(session, num_params, param1, param2, db);
    } else if (strcmp(command, "FRIEND_LIST") == 0) {
        handle_friend_list(session, num_params, param1, db);
    } else if (strcmp(command, "FRIEND_REQUESTS") == 0) {
        handle_friend_requests(session, num_params, param1, db);
    }
    // Timer
    else if (strcmp(command, "START_TIMER") == 0) {
        handle_start_timer(session, param1, db);
    } else if (strcmp(command, "STOP_TIMER") == 0) {
        handle_stop_timer(session, param1, db);
    } else if (strcmp(command, "PAUSE_TIMER") == 0) {
        handle_pause_timer(session, param1, db);
    } else if (strcmp(command, "RESUME_TIMER") == 0) {
        handle_resume_timer(session, param1, db);
    } else if (strcmp(command, "GET_TIME") == 0) {
        handle_get_time(session, param1, db);
    }
    // Game control
    else if (strcmp(command, "PAUSE") == 0) {
        handle_pause(session, num_params, param1, param2, db);
    } else if (strcmp(command, "RESUME") == 0) {
        handle_resume(session, num_params, param1, param2, db);
    } else if (strcmp(command, "DRAW") == 0) {
        handle_draw_request(session, num_params, param1, param2, db);
    } else if (strcmp(command, "DRAW_ACCEPT") == 0) {
        handle_draw_accept(session, num_params, param1, param2, db);
    } else if (strcmp(command, "DRAW_DECLINE") == 0) {
        handle_draw_decline(session, num_params, param1, param2, db);
    } else if (strcmp(command, "REMATCH") == 0) {
        handle_rematch_request(session, num_params, param1, param2, db);
    } else if (strcmp(command, "REMATCH_ACCEPT") == 0) {
        handle_rematch_accept(session, num_params, param1, param2, db);
    } else if (strcmp(command, "REMATCH_DECLINE") == 0) {
        handle_rematch_decline(session, num_params, param1, param2, db);
    }
    // Matchmaking
    else if (strcmp(command, "JOIN_MATCHMAKING") == 0 && num_params >= 2) {
        handle_join_matchmaking(session, param1, db);
    } else if (strcmp(command, "LEAVE_MATCHMAKING") == 0 && num_params >= 2) {
        handle_leave_matchmaking(session, param1, db);
    } else if (strcmp(command, "MMJOIN") == 0 && num_params >= 4) {
        char payload[256];
        snprintf(payload, sizeof(payload), "%s %s %s", param1, param2, param3);
        char resp[256];
        memset(resp, 0, sizeof(resp));
        handle_mmjoin(payload, resp, sizeof(resp));
        send(session->socket_fd, resp, strlen(resp), 0);
    } else if (strcmp(command, "MMSTATUS") == 0 && num_params >= 2) {
        char resp[256];
        memset(resp, 0, sizeof(resp));
        handle_mmstatus(param1, resp, sizeof(resp));
        send(session->socket_fd, resp, strlen(resp), 0);
    } else if (strcmp(command, "MMCANCEL") == 0 && num_params >= 2) {
        char resp[256];
        memset(resp, 0, sizeof(resp));
        handle_mmcancel(param1, resp, sizeof(resp));
        send(session->socket_fd, resp, strlen(resp), 0);
    }
    // ELO Leaderboard
    else if (strcmp(command, "GET_LEADERBOARD") == 0) {
        char output[2048] = {0};
        int limit = 20;
        if (num_params >= 2) limit = atoi(param1);
        elo_get_leaderboard(db, output, sizeof(output), limit);
        send(session->socket_fd, output, strlen(output), 0);
    }
    // ELO History
    else if (strcmp(command, "GET_ELO_HISTORY") == 0) {
        char output[2048] = {0};
        int user_id = atoi(param1);
        elo_get_history(db, user_id, output, sizeof(output));
        send(session->socket_fd, output, strlen(output), 0);
    } else {
        char error[128];
        sprintf(error, "ERROR|Unknown command: %s\n", command);
        send(session->socket_fd, error, strlen(error), 0);
        printf("[Protocol] Unknown command: %s\n", command);
    }
}
