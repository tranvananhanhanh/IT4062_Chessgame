#include "server_core.h"
#include "game.h"
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

// Global variables
GameManager game_manager;
PGconn *db_conn = NULL;

// Load environment variables from .env file
void load_env_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("[Main] No .env file found, using system environment variables\n");
        return;
    }
    
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;
        
        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;
        
        // Parse KEY=VALUE
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *key = line;
            char *value = eq + 1;
            
            // Set environment variable
            setenv(key, value, 1);
        }
    }
    
    fclose(file);
    printf("[Main] Environment variables loaded from %s\n", filename);
}

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("\n[Main] Received signal %d\n", sig);
    server_shutdown();
    exit(0);
}

int main() {
    // Load environment variables from .env file
    load_env_file(".env");
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize server
    if (server_init(&db_conn) != 0) {
        fprintf(stderr, "[Main] Server initialization failed\n");
        return 1;
    }
    
    printf("[Server] Ready to accept connections!\n\n");
    
    // Track last cleanup time
    time_t last_cleanup = time(NULL);
    
    while (1) {
        // Cleanup finished matches every 30 seconds
        time_t now = time(NULL);
        if (now - last_cleanup > 30) {
            game_manager_cleanup_finished_matches(&game_manager);
            last_cleanup = now;
        }
        
        // Start server (blocking call)
        server_start();
    }
    
    // Cleanup (will only reach here on error)
    server_shutdown();
    
    return 0;
}
