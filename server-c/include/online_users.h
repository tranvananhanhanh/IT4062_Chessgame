#ifndef ONLINE_USERS_H
#define ONLINE_USERS_H

#include "client_session.h"
#include <pthread.h>

#define MAX_ONLINE_USERS 1024

typedef struct {
    char username[64];
    int user_id;
    int socket_fd;
} OnlineUserEntry;

typedef struct {
    OnlineUserEntry entries[MAX_ONLINE_USERS];
    int count;
    pthread_mutex_t lock;
} OnlineUsers;

extern OnlineUsers online_users;

// Returns socket fd for user_id, or -1 if not found
int online_users_get_fd(OnlineUsers *users, const char *user_id);
// Add/remove helpers (not used by chat, but useful for login/logout)
void online_users_add(OnlineUsers *users, int user_id, const char *username, int socket_fd);
void online_users_remove(OnlineUsers *users, int user_id);

#endif
