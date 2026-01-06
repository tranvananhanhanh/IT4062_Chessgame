#include "server_core.h"
#include "game.h"
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

// Global variables
GameManager game_manager;
PGconn *db_conn = NULL;
pid_t bot_pid = -1;  // Bot process ID

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("\n[Main] Received signal %d\n", sig);
    
    // Shutdown bot server first
    if (bot_pid > 0) {
        printf("[Main] Stopping bot server (PID: %d)...\n", bot_pid);
        kill(bot_pid, SIGTERM);
        waitpid(bot_pid, NULL, 0);
        printf("[Main] Bot server stopped\n");
    }
    
    server_shutdown();
    exit(0);
}

int main() {
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Start bot server
    printf("[Main] Starting bot server...\n");
    bot_pid = fork();
    
    if (bot_pid == 0) {
        // Child process: run bot server
        chdir("../bot");  // Change to bot directory
        execl("/bin/bash", "bash", "-c", 
              "source .venv/bin/activate && python3 bot_socket_server.py", 
              (char*)NULL);
        
        // If exec fails
        perror("[Error] Failed to start bot server");
        exit(1);
    } else if (bot_pid < 0) {
        perror("[Error] Fork failed for bot server");
        return 1;
    }
    
    // Parent process: give bot time to start
    printf("[Main] Bot server started (PID: %d)\n", bot_pid);
    printf("[Main] Waiting for bot server to initialize...\n");
    sleep(2);
    
    // Initialize server
    if (server_init(&db_conn) != 0) {
        fprintf(stderr, "[Main] Server initialization failed\n");
        if (bot_pid > 0) {
            kill(bot_pid, SIGTERM);
            waitpid(bot_pid, NULL, 0);
        }
        return 1;
    }
    
    printf("[Server] Ready to accept connections!\n\n");
    
    // Track last cleanup time
    time_t last_cleanup = time(NULL);
    
    while (1) {
        // Check if bot process is still alive
        int status;
        pid_t result = waitpid(bot_pid, &status, WNOHANG);
        if (result != 0) {
            fprintf(stderr, "[Warning] Bot server terminated unexpectedly\n");
            // Could restart bot here if needed
        }
        
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
    if (bot_pid > 0) {
        kill(bot_pid, SIGTERM);
        waitpid(bot_pid, NULL, 0);
    }
    server_shutdown();
    
    return 0;
}
