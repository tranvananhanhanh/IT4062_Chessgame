#include "client_session.h"
#include "online_users.h"
#include "history.h"
#include "game.h"
#include "timer.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

// Extern globals
extern GameManager game_manager;
extern PGconn *db_conn;

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

        if (match->white_player.socket_fd == session->socket_fd) {
            match->white_player.is_online = 0;
        } else if (match->black_player.socket_fd == session->socket_fd) {
            match->black_player.is_online = 0;
        }

        /* Notify opponent (if any) */
        char msg[] = "OPPONENT_DISCONNECTED\n";
        broadcast_to_match(match, msg, session->socket_fd);

        /* If game is in progress, award win to the remaining player */
        if (match->status == GAME_PLAYING) {
            int winner_id, loser_id;
            PlayerColor winner_color;

            if (match->white_player.socket_fd == session->socket_fd) {
                winner_id = match->black_player.user_id;
                winner_color = COLOR_BLACK;
            } else {
                winner_id = match->white_player.user_id;
                winner_color = COLOR_WHITE;
            }
            loser_id = session->user_id;

            match->status = GAME_FINISHED;
            match->result = (winner_color == COLOR_WHITE) ? RESULT_WHITE_WIN : RESULT_BLACK_WIN;
            match->winner_id = winner_id;
            match->end_time = time(NULL);

            /* Stop game timer */
            timer_manager_stop_timer(&game_manager.timer_manager, match->match_id);

            /* Persist result */
            const char *result_str = (winner_color == COLOR_WHITE) ? "white_win" : "black_win";
            history_update_match_result(db_conn, match->match_id, result_str, winner_id);

            /* Update ELO if both players are valid */
            if (winner_id > 0 && loser_id > 0) {
                stats_update_elo(db_conn, winner_id, loser_id);
            }

            /* Notify remaining player about the win */
            char end_msg[BUFFER_SIZE];
            snprintf(end_msg, sizeof(end_msg),
                     "GAME_END|opponent_disconnected|winner:%d\n", winner_id);
            broadcast_to_match(match, end_msg, session->socket_fd);
        } else {
            /* Otherwise just mark aborted */
            match->status = GAME_ABORTED;
        }

        pthread_mutex_unlock(&match->lock);
    }

    printf("[Session] Client %d disconnected (user: %s)\n",
           session->socket_fd,
           session->user_id > 0 ? session->username : "anonymous");
}
