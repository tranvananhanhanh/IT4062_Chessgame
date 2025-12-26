#ifndef CLIENT_SESSION_H
#define CLIENT_SESSION_H

// Forward declaration to avoid circular include
struct GameMatch;

// Client session structure
typedef struct {
    int socket_fd;
    int user_id;
    char username[64];
    struct GameMatch *current_match;
    int is_bot_mode; // Thêm biến này để giữ session bot không bị disconnect
} ClientSession;

// Session management functions
ClientSession* client_session_create(int socket_fd);
void client_session_destroy(ClientSession *session);
void client_session_handle_disconnect(ClientSession *session);

#endif // CLIENT_SESSION_H
