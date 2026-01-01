#include "client_session.h"
#include "online_users.h"
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

        if (match->white_player.socket_fd == session->socket_fd) {
            match->white_player.is_online = 0;
        } else if (match->black_player.socket_fd == session->socket_fd) {
            match->black_player.is_online = 0;
        }

        /* Notify opponent (if any) */
        char msg[] = "OPPONENT_DISCONNECTED\n";
        broadcast_to_match(match, msg, session->socket_fd);

        /* Abort game */
        match->status = GAME_ABORTED;

        pthread_mutex_unlock(&match->lock);
    }

    printf("[Session] Client %d disconnected (user: %s)\n",
           session->socket_fd,
           session->user_id > 0 ? session->username : "anonymous");
}
