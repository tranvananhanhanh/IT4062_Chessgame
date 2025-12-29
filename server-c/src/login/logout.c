#include "login.h"
#include "online_users.h"
#include "database.h"
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>

// Xử lý logout
void handle_logout(ClientSession *session) {
    // Remove from online users
    extern OnlineUsers online_users;
    online_users_remove(&online_users, session->user_id);
    
    // Update user state to offline in database
    extern PGconn *db_conn;
    char update_query[256];
    snprintf(update_query, sizeof(update_query), 
        "UPDATE users SET state = 'offline' WHERE user_id = %d", session->user_id);
    PGresult *res = PQexec(db_conn, update_query);
    PQclear(res);
    
    // Xoá thông tin user khỏi session, đóng socket nếu cần
    session->user_id = 0;
    session->current_match = NULL;
    memset(session->username, 0, sizeof(session->username));
    // Gửi response về client
    const char *resp = "LOGOUT_SUCCESS\n";
    send(session->socket_fd, resp, strlen(resp), 0);
    printf("[Auth] User logout, session reset\n");
}
