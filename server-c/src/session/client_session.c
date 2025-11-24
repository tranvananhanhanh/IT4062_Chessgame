#include "client_session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    if (session->current_match != NULL) {
        pthread_mutex_lock(&session->current_match->lock);
        
        // Mark player as offline
        if (session->current_match->white_player.socket_fd == session->socket_fd) {
            session->current_match->white_player.is_online = 0;
        } else if (session->current_match->black_player.socket_fd == session->socket_fd) {
            session->current_match->black_player.is_online = 0;
        }
        
        // Notify opponent
        char msg[] = "OPPONENT_DISCONNECTED\n";
        broadcast_to_match(session->current_match, msg, session->socket_fd);
        
        // Abort game
        session->current_match->status = GAME_ABORTED;
        
        pthread_mutex_unlock(&session->current_match->lock);
    }
    
    printf("[Session] Client %d disconnected (user: %s)\n", 
           session->socket_fd, 
           session->user_id > 0 ? session->username : "anonymous");
}
