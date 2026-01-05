#ifndef TIMER_H
#define TIMER_H

#include <pthread.h>
#include <time.h>
#include "client_session.h"
#include <libpq-fe.h>

// Timer callback function type
typedef void (*timer_callback_t)(int match_id);

// Timer structure
typedef struct {
    int match_id;
    time_t start_time;
    int duration_seconds;
    int remaining_seconds;
    int is_active;
    int is_paused;
    pthread_t thread;
    timer_callback_t callback;
    pthread_mutex_t lock;
} GameTimer;

// Timer manager
typedef struct {
    GameTimer *timers[100];  // Max 100 concurrent timers
    int timer_count;
    pthread_mutex_t lock;
} TimerManager;

// Function declarations
void timer_manager_init(TimerManager *manager);
GameTimer* timer_manager_create_timer(TimerManager *manager, int match_id,
                                     int duration_minutes, timer_callback_t callback);
int timer_manager_stop_timer(TimerManager *manager, int match_id);
int timer_manager_pause_timer(TimerManager *manager, int match_id);
int timer_manager_resume_timer(TimerManager *manager, int match_id);
int timer_manager_get_remaining_time(TimerManager *manager, int match_id);
void timer_manager_cleanup(TimerManager *manager);

void handle_start_timer(ClientSession *session, char *param1, PGconn *db);
void handle_stop_timer(ClientSession *session, char *param1, PGconn *db);
void handle_pause_timer(ClientSession *session, char *param1, PGconn *db);
void handle_resume_timer(ClientSession *session, char *param1, PGconn *db);
void handle_get_time(ClientSession *session, char *param1, PGconn *db);

#endif // TIMER_H
