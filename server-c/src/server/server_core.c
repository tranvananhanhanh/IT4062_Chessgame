#include "server_core.h"
#include "client_session.h"
#include "protocol_handler.h"
#include "game.h"
#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// External global variables
extern GameManager game_manager;
extern PGconn *db_conn;

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
    
    return 0;
}

// Start server and accept connections
void server_start() {
    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[Error] Socket creation failed");
        return;
    }
    
    // Reuse address
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind to port
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[Error] Bind failed");
        close(server_fd);
        return;
    }
    
    // Listen
    if (listen(server_fd, 10) < 0) {
        perror("[Error] Listen failed");
        close(server_fd);
        return;
    }
    
    printf("[Server] Listening on port %d...\n", PORT);
    printf("[Server] Ready to accept connections!\n\n");
    
    // ✅ Track last cleanup time
    time_t last_cleanup = time(NULL);
    time_t last_force_cleanup = time(NULL);
    
    // Accept connections loop
    while (1) {
        // ✅ Periodic cleanup (every 30 seconds)
        time_t now = time(NULL);
        if (now - last_cleanup >= 30) {
            game_manager_cleanup_finished_matches(&game_manager);
            last_cleanup = now;
        }
        
        // ✅ Force cleanup stale matches (every 5 minutes)
        if (now - last_force_cleanup >= 300) {
            game_manager_force_cleanup_stale_matches(&game_manager);
            last_force_cleanup = now;
        }
        
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            perror("[Error] Accept failed");
            continue;
        }
        
        printf("[Server] New connection from %s:%d (fd: %d)\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port),
               client_fd);
        
        // Create session
        ClientSession *session = client_session_create(client_fd);
        if (session == NULL) {
            fprintf(stderr, "[Error] Failed to create session for client %d\n", client_fd);
            close(client_fd);
            continue;
        }
        
        // Create thread for client
        pthread_t thread;
        if (pthread_create(&thread, NULL, client_handler, session) != 0) {
            fprintf(stderr, "[Error] Failed to create thread for client %d\n", client_fd);
            client_session_destroy(session);
            close(client_fd);
            continue;
        }
        
        pthread_detach(thread);
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
