#include "timer.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

void timer_manager_init(TimerManager *manager) {
    manager->timer_count = 0;
    pthread_mutex_init(&manager->lock, NULL);
    memset(manager->timers, 0, sizeof(manager->timers));
}

static void* timer_thread_func(void *arg) {
    GameTimer *timer = (GameTimer *)arg;

    while (1) {
        sleep(1);  // Check every second

        pthread_mutex_lock(&timer->lock);

        if (!timer->is_active) {
            pthread_mutex_unlock(&timer->lock);
            break;
        }

        if (timer->is_paused) {
            pthread_mutex_unlock(&timer->lock);
            continue;
        }

        timer->remaining_seconds--;

        if (timer->remaining_seconds <= 0) {
            // Time's up - call callback
            if (timer->callback) {
                timer->callback(timer->match_id);
            }
            timer->is_active = 0;
            pthread_mutex_unlock(&timer->lock);
            break;
        }

        pthread_mutex_unlock(&timer->lock);
    }

    return NULL;
}

GameTimer* timer_manager_create_timer(TimerManager *manager, int match_id,
                                     int duration_minutes, timer_callback_t callback) {
    pthread_mutex_lock(&manager->lock);

    if (manager->timer_count >= 100) {
        pthread_mutex_unlock(&manager->lock);
        return NULL;
    }

    // Check if timer already exists for this match
    for (int i = 0; i < manager->timer_count; i++) {
        if (manager->timers[i] && manager->timers[i]->match_id == match_id) {
            pthread_mutex_unlock(&manager->lock);
            return NULL;  // Timer already exists
        }
    }

    GameTimer *timer = (GameTimer *)malloc(sizeof(GameTimer));
    if (!timer) {
        pthread_mutex_unlock(&manager->lock);
        return NULL;
    }

    timer->match_id = match_id;
    timer->start_time = time(NULL);
    timer->duration_seconds = duration_minutes * 60;
    timer->remaining_seconds = timer->duration_seconds;
    timer->is_active = 1;
    timer->is_paused = 0;
    timer->callback = callback;
    pthread_mutex_init(&timer->lock, NULL);

    // Start timer thread
    if (pthread_create(&timer->thread, NULL, timer_thread_func, timer) != 0) {
        free(timer);
        pthread_mutex_unlock(&manager->lock);
        return NULL;
    }

    // Add to manager
    manager->timers[manager->timer_count] = timer;
    manager->timer_count++;

    pthread_mutex_unlock(&manager->lock);

    printf("[Timer] Created timer for match %d (%d minutes)\n", match_id, duration_minutes);
    return timer;
}

int timer_manager_stop_timer(TimerManager *manager, int match_id) {
    pthread_mutex_lock(&manager->lock);

    for (int i = 0; i < manager->timer_count; i++) {
        if (manager->timers[i] && manager->timers[i]->match_id == match_id) {
            GameTimer *timer = manager->timers[i];

            pthread_mutex_lock(&timer->lock);
            timer->is_active = 0;
            pthread_mutex_unlock(&timer->lock);

            // Wait for thread to finish
            pthread_join(timer->thread, NULL);

            // Remove from manager
            free(timer);
            for (int j = i; j < manager->timer_count - 1; j++) {
                manager->timers[j] = manager->timers[j + 1];
            }
            manager->timers[manager->timer_count - 1] = NULL;
            manager->timer_count--;

            pthread_mutex_unlock(&manager->lock);
            printf("[Timer] Stopped timer for match %d\n", match_id);
            return 1;
        }
    }

    pthread_mutex_unlock(&manager->lock);
    return 0;
}

int timer_manager_pause_timer(TimerManager *manager, int match_id) {
    pthread_mutex_lock(&manager->lock);

    for (int i = 0; i < manager->timer_count; i++) {
        if (manager->timers[i] && manager->timers[i]->match_id == match_id) {
            pthread_mutex_lock(&manager->timers[i]->lock);
            manager->timers[i]->is_paused = 1;
            pthread_mutex_unlock(&manager->timers[i]->lock);

            pthread_mutex_unlock(&manager->lock);
            printf("[Timer] Paused timer for match %d\n", match_id);
            return 1;
        }
    }

    pthread_mutex_unlock(&manager->lock);
    return 0;
}

int timer_manager_resume_timer(TimerManager *manager, int match_id) {
    pthread_mutex_lock(&manager->lock);

    for (int i = 0; i < manager->timer_count; i++) {
        if (manager->timers[i] && manager->timers[i]->match_id == match_id) {
            pthread_mutex_lock(&manager->timers[i]->lock);
            manager->timers[i]->is_paused = 0;
            pthread_mutex_unlock(&manager->timers[i]->lock);

            pthread_mutex_unlock(&manager->lock);
            printf("[Timer] Resumed timer for match %d\n", match_id);
            return 1;
        }
    }

    pthread_mutex_unlock(&manager->lock);
    return 0;
}

int timer_manager_get_remaining_time(TimerManager *manager, int match_id) {
    pthread_mutex_lock(&manager->lock);

    for (int i = 0; i < manager->timer_count; i++) {
        if (manager->timers[i] && manager->timers[i]->match_id == match_id) {
            int remaining = manager->timers[i]->remaining_seconds;
            pthread_mutex_unlock(&manager->lock);
            return remaining;
        }
    }

    pthread_mutex_unlock(&manager->lock);
    return 0;
}

void timer_manager_cleanup(TimerManager *manager) {
    pthread_mutex_lock(&manager->lock);

    for (int i = 0; i < manager->timer_count; i++) {
        if (manager->timers[i]) {
            GameTimer *timer = manager->timers[i];

            pthread_mutex_lock(&timer->lock);
            timer->is_active = 0;
            pthread_mutex_unlock(&timer->lock);

            pthread_join(timer->thread, NULL);
            free(timer);
        }
    }

    manager->timer_count = 0;
    pthread_mutex_unlock(&manager->lock);
    pthread_mutex_destroy(&manager->lock);
}