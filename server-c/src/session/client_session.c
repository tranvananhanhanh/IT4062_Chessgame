#include "client_session.h"
#include "online_users.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include "game.h"

ClientSession* client_session_create(int socket_fd) {
    ClientSession *session = (ClientSession*)malloc(sizeof(ClientSession));
    if (session == NULL) {
        return NULL;
    }

    session->socket_fd = socket_fd;
    session->user_id = 0;
    memset(session->username, 0, sizeof(session->username));
    session->current_match = NULL;

    return session;
}

void client_session_destroy(ClientSession *session) {
    if (session != NULL) {
        free(session);
    }
}

void client_session_handle_disconnect(ClientSession *session) {
    if (!session) return;

    /* ===== REMOVE USER FROM ONLINE USERS ===== */
    if (session->user_id > 0) {
        extern OnlineUsers online_users;
        online_users_remove(&online_users, session->user_id);

        printf("[Session] Removed user %d (%s) from online users\n",
               session->user_id, session->username);
    }

    /* ===== HANDLE CURRENT MATCH ===== */
    if (session->current_match != NULL) {
        GameMatch *match = session->current_match;

        pthread_mutex_lock(&match->lock);

        int disconnected_player_id = 0;
        int remaining_player_id = 0;
        int disconnected_is_white = 0;

        if (match->white_player.socket_fd == session->socket_fd) {
            match->white_player.is_online = 0;
            disconnected_player_id = match->white_player.user_id;
            remaining_player_id = match->black_player.user_id;
            disconnected_is_white = 1;
        } else if (match->black_player.socket_fd == session->socket_fd) {
            match->black_player.is_online = 0;
            disconnected_player_id = match->black_player.user_id;
            remaining_player_id = match->white_player.user_id;
            disconnected_is_white = 0;
        }

        /* Notify opponent (if any) */
        char msg[] = "OPPONENT_DISCONNECTED\n";
        broadcast_to_match(match, msg, session->socket_fd);

        /* ===== UPDATE MATCH RESULT (REMAINING PLAYER WINS) ===== */
        if (match->status == GAME_PLAYING && disconnected_player_id > 0 && remaining_player_id > 0) {
            extern PGconn *db_conn;
            
            // Set winner to the remaining player
            match->winner_id = remaining_player_id;
            match->result = disconnected_is_white ? RESULT_BLACK_WIN : RESULT_WHITE_WIN;
            match->status = GAME_FINISHED;
            
            // Update database
            const char *result_str = disconnected_is_white ? "black_win" : "white_win";
            history_update_match_result(db_conn, match->match_id, result_str, remaining_player_id);
            
            // Update ELO: remaining player wins, disconnected player loses
            if (db_conn != NULL) {
                stats_update_elo(db_conn, remaining_player_id, disconnected_player_id);
                printf("[Session] ELO updated: %d wins (+30), %d loses (-30) due to disconnect\n",
                       remaining_player_id, disconnected_player_id);
            }
            
            // Send game end notification to remaining player
            char game_end_msg[BUFFER_SIZE];
            sprintf(game_end_msg, "GAME_END|opponent_disconnect|winner:%d\n", remaining_player_id);
            
            int opponent_fd = disconnected_is_white ? match->black_player.socket_fd : match->white_player.socket_fd;
            if (opponent_fd > 0) {
                send(opponent_fd, game_end_msg, strlen(game_end_msg), 0);
                printf("[Session] Sent win notification to remaining player (fd=%d)\n", opponent_fd);
            }
            
            printf("[Session] Match %d ended due to player disconnect: winner=%d, loser=%d\n",
                   match->match_id, remaining_player_id, disconnected_player_id);
        } else {
            /* Abort game if not in PLAYING state or invalid players */
            match->status = GAME_ABORTED;
        }

        pthread_mutex_unlock(&match->lock);
    }

    printf("[Session] Client %d disconnected (user: %s)\n",
           session->socket_fd,
           session->user_id > 0 ? session->username : "anonymous");
}
