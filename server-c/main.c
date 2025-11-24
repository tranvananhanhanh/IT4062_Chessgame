#include "server_core.h"
#include "game.h"
#include <stdio.h>
#include <signal.h>

// Global variables
GameManager game_manager;
PGconn *db_conn = NULL;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("\n[Main] Received signal %d\n", sig);
    server_shutdown();
    exit(0);
}

int main() {
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize server
    if (server_init(&db_conn) != 0) {
        fprintf(stderr, "[Main] Server initialization failed\n");
        return 1;
    }
    
    // Start server (blocking call)
    server_start();
    
    // Cleanup (will only reach here on error)
    server_shutdown();
    
    return 0;
}
