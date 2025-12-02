// protocol_handler.c - Protocol command router
#include "protocol_handler.h"
#include "match.h"
#include "friend.h"
#include "bot.h"
#include "control.h"
#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// External global variable
extern GameManager game_manager;

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
