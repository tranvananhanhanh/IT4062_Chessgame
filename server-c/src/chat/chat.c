#include "chat.h"
#include "server_core.h"
#include "online_users.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// Extern bảng online_users từ server_core
extern OnlineUsers online_users;

void handle_chat(ClientSession *session, int num_params, const char *to_user, const char *message) {
    if (num_params < 3 || !to_user || !message) return;
    // Tìm socket của user nhận
    int to_fd = online_users_get_fd(&online_users, to_user);
    if (to_fd <= 0) {
        // Không gửi error, chỉ bỏ qua (theo mô tả)
        return;
    }
    // Lấy user gửi (dùng user_id thay vì username)
    char from_id[16];
    snprintf(from_id, sizeof(from_id), "%d", session->user_id);
    // Đóng gói và gửi CHAT_FROM|from_id|message\n
    char buf[1024];
    snprintf(buf, sizeof(buf), "CHAT_FROM|%s|%s\n", from_id, message);
    send(to_fd, buf, strlen(buf), 0);
}
