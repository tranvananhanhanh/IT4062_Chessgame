#include "game_chat.h"
#include "online_users.h"
#include "database.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

// Extern bảng online_users từ server_core
extern OnlineUsers online_users;

// Get opponent user_id and socket_fd from match_id
// Query: SELECT white_user_id, black_user_id FROM matches WHERE match_id = ?
static void get_opponent_info(PGconn *db, int match_id, int my_user_id, 
                              int *opponent_id, int *opponent_fd, char *opponent_username) {
    // Simple query to get both players from match
    char query[256];
    snprintf(query, sizeof(query), 
             "SELECT white_user_id, black_user_id FROM matches WHERE match_id = %d", 
             match_id);
    
    PGresult *res = PQexec(db, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        *opponent_id = -1;
        return;
    }
    
    if (PQntuples(res) == 0) {
        PQclear(res);
        *opponent_id = -1;
        return;
    }
    
    int white_id = atoi(PQgetvalue(res, 0, 0));
    int black_id = atoi(PQgetvalue(res, 0, 1));
    PQclear(res);
    
    // Determine opponent
    *opponent_id = (my_user_id == white_id) ? black_id : white_id;
    
    // Get opponent's socket fd and username using user_id
    char user_id_str[16];
    snprintf(user_id_str, sizeof(user_id_str), "%d", *opponent_id);
    
    *opponent_fd = online_users_get_fd(&online_users, user_id_str);
    
    // Get opponent username
    snprintf(query, sizeof(query), 
             "SELECT name FROM users WHERE user_id = %d", 
             *opponent_id);
    
    res = PQexec(db, query);
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        strncpy(opponent_username, PQgetvalue(res, 0, 0), 63);
        opponent_username[63] = '\0';
    }
    PQclear(res);
}

void handle_game_chat(ClientSession *session, int match_id, const char *message, PGconn *db) {
    if (!session || !message || !db) return;
    
    printf("[GameChat] User %d sending message to match %d: %s\n", session->user_id, match_id, message);
    
    int opponent_id = -1;
    int opponent_fd = -1;
    char opponent_username[64] = {0};
    
    // Get opponent info from match
    get_opponent_info(db, match_id, session->user_id, &opponent_id, &opponent_fd, opponent_username);
    
    printf("[GameChat] Opponent ID: %d, FD: %d, Name: %s\n", opponent_id, opponent_fd, opponent_username);
    
    if (opponent_fd <= 0) {
        // Opponent not connected, silently ignore
        printf("[GameChat] Opponent not connected (fd=%d), ignoring message\n", opponent_fd);
        return;
    }
    
    // Get sender's username
    char query[256];
    snprintf(query, sizeof(query), 
             "SELECT name FROM users WHERE user_id = %d", 
             session->user_id);
    
    PGresult *res = PQexec(db, query);
    char sender_username[64] = {0};
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        strncpy(sender_username, PQgetvalue(res, 0, 0), 63);
        sender_username[63] = '\0';
    }
    PQclear(res);
    
    // Format and send: GAME_CHAT_FROM|sender_username|message
    char buf[1024];
    snprintf(buf, sizeof(buf), "GAME_CHAT_FROM|%s|%s\n", sender_username, message);
    
    printf("[GameChat] Sending to opponent (fd=%d): %s\n", opponent_fd, buf);
    
    if (send(opponent_fd, buf, strlen(buf), 0) < 0) {
        // Failed to send, but don't report error to client
        perror("send_game_chat");
    }
}
