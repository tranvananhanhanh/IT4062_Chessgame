#include "server_core.h"
#include "client_session.h"
#include "protocol_handler.h"
#include "game.h"
#include "history.h"
#include "online_users.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#define MAX_CLIENTS 1024

// External global variables
extern GameManager game_manager;
extern PGconn *db_conn;
extern OnlineUsers online_users;

// Client handler thread
void* client_handler(void *arg) {
    ClientSession *session = (ClientSession*)arg;
    int client_fd = session->socket_fd;
    char buffer[BUFFER_SIZE];
    
    printf("[Server] Client %d connected\n", client_fd);
    
    // Send welcome message
    const char *welcome = "WELCOME|Chess Server v1.0\n";
    send(client_fd, welcome, strlen(welcome), 0);
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_read <= 0) {
            // Client disconnected
            client_session_handle_disconnect(session);
            break;
        }
        
        // Remove newline
        buffer[strcspn(buffer, "\n")] = 0;
        
        printf("[Server] Received from client %d: %s\n", client_fd, buffer);
        
        // Handle command
        protocol_handle_command(session, buffer, db_conn);
    }
    
    close(client_fd);
    client_session_destroy(session);
    return NULL;
}

// Server initialization
int server_init(PGconn **db_connection) {
    printf("========================================\n");
    printf("     Chess Game Server Starting...     \n");
    printf("========================================\n\n");
    
    // Connect to database
    *db_connection = db_connect();
    if (*db_connection == NULL) {
        fprintf(stderr, "[Error] Failed to connect to database\n");
        return -1;
    }
    
    // Initialize game manager
    game_manager_init(&game_manager);
    printf("[Server] Game manager initialized\n");

    pthread_mutex_init(&online_users.lock, NULL);
    online_users.count = 0;
    
    return 0;
}

// Start server and accept connections using poll for I/O multiplexing
void server_start() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[Error] Socket creation failed");
        return;
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[Error] Bind failed");
        close(server_fd);
        return;
    }
    if (listen(server_fd, 10) < 0) {
        perror("[Error] Listen failed");
        close(server_fd);
        return;
    }
    printf("[Server] Listening on port %d...\n", PORT);
    printf("[Server] Ready to accept connections!\n\n");

    struct pollfd fds[MAX_CLIENTS];
    ClientSession* sessions[MAX_CLIENTS] = {0};
    int nfds = 1;
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    time_t last_cleanup = time(NULL);
    time_t last_force_cleanup = time(NULL);

    while (1) {
        time_t now = time(NULL);
        if (now - last_cleanup >= 30) {
            game_manager_cleanup_finished_matches(&game_manager);
            last_cleanup = now;
        }
        if (now - last_force_cleanup >= 300) {
            game_manager_force_cleanup_stale_matches(&game_manager);
            last_force_cleanup = now;
        }
        int poll_count = poll(fds, nfds, 1000); // 1s timeout
        if (poll_count < 0) {
            perror("[Error] poll failed");
            break;
        }
        // New connection
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd >= 0 && nfds < MAX_CLIENTS) {
                fds[nfds].fd = client_fd;
                fds[nfds].events = POLLIN;
                sessions[nfds] = client_session_create(client_fd);
                nfds++;
                printf("[Server] New connection from %s:%d (fd: %d)\n",
                       inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port),
                       client_fd);
                // Send welcome message
                const char *welcome = "WELCOME|Chess Server v1.0\n";
                send(client_fd, welcome, strlen(welcome), 0);
            }
        }
        // Handle client IO
        for (int i = 1; i < nfds; ++i) {
            if (fds[i].revents & POLLIN) {
                char buffer[BUFFER_SIZE] = {0};
                int bytes_read = recv(fds[i].fd, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_read <= 0) {
                    // Client disconnected
                    client_session_handle_disconnect(sessions[i]);
                    close(fds[i].fd);
                    client_session_destroy(sessions[i]);
                    // Remove from poll list
                    for (int j = i; j < nfds - 1; ++j) {
                        fds[j] = fds[j+1];
                        sessions[j] = sessions[j+1];
                    }
                    nfds--;
                    i--;
                } else {
                    buffer[strcspn(buffer, "\n")] = 0;
                    printf("[Server] Received from client %d: %s\n", fds[i].fd, buffer);
                    protocol_handle_command(sessions[i], buffer, db_conn);
                }
            }
        }
    }
    close(server_fd);
}

// Shutdown server
void server_shutdown() {
    printf("\n[Server] Shutting down...\n");
    
    if (db_conn != NULL) {
        db_disconnect(db_conn);
        printf("[Server] Database connection closed\n");
    }
    
    printf("[Server] Shutdown complete\n");
}
