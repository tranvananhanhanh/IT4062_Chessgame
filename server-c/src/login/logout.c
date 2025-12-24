#include "login.h"
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>

// Xử lý logout
void handle_logout(ClientSession *session) {
    // Xoá thông tin user khỏi session, đóng socket nếu cần
    session->user_id = 0;
    session->current_match = NULL;
    memset(session->username, 0, sizeof(session->username));
    // Gửi response về client
    const char *resp = "LOGOUT_SUCCESS\n";
    send(session->socket_fd, resp, strlen(resp), 0);
    printf("[Auth] User logout, session reset\n");
}
