#include "online_users.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>


// Global instance for use by server and chat
OnlineUsers online_users = {.count = 0, .lock = PTHREAD_MUTEX_INITIALIZER};

int online_users_get_fd(OnlineUsers *users, const char *user_id_str) {
    int fd = -1;
    int user_id = atoi(user_id_str);
    pthread_mutex_lock(&users->lock);
    for (int i = 0; i < users->count; ++i) {
        if (users->entries[i].user_id == user_id) {
            fd = users->entries[i].socket_fd;
            break;
        }
    }
    pthread_mutex_unlock(&users->lock);
    return fd;
}

void online_users_add(OnlineUsers *users, int user_id, const char *username, int socket_fd) {
    pthread_mutex_lock(&users->lock);
    for (int i = 0; i < users->count; ++i) {
        if (users->entries[i].user_id == user_id) {
            users->entries[i].socket_fd = socket_fd;
            pthread_mutex_unlock(&users->lock);
            return;
        }
    }
    if (users->count < MAX_ONLINE_USERS) {
        users->entries[users->count].user_id = user_id;
        strncpy(users->entries[users->count].username, username, 63);
        users->entries[users->count].username[63] = '\0';
        users->entries[users->count].socket_fd = socket_fd;
        users->count++;
    }
    pthread_mutex_unlock(&users->lock);
}

void online_users_remove(OnlineUsers *users, int user_id) {
    pthread_mutex_lock(&users->lock);
    for (int i = 0; i < users->count; ++i) {
        if (users->entries[i].user_id == user_id) {
            users->entries[i] = users->entries[users->count - 1];
            users->count--;
            break;
        }
    }
    pthread_mutex_unlock(&users->lock);
}
