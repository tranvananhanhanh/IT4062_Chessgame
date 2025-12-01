#include "matchmaking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct MMPlayer {
    char id[64];        // username
    int elo;
    char time_mode[16]; // BLITZ / RAPID...

    int in_match;       // 0: chưa, 1: đã ghép
    char match_id[64];  // M000001
    char opponent[64];  // username đối thủ
    char color[8];      // "white" / "black"

    struct MMPlayer *next;
} MMPlayer;

static MMPlayer *g_players = NULL;   // tất cả player từng join
static MMPlayer *g_waiting = NULL;   // hàng chờ
static int g_next_match_id = 1;

static MMPlayer *find_player(const char *id) {
    MMPlayer *p = g_players;
    while (p) {
        if (strcmp(p->id, id) == 0) return p;
        p = p->next;
    }
    return NULL;
}

static void add_player(MMPlayer *p) {
    p->next = g_players;
    g_players = p;
}

static void add_waiting(MMPlayer *p) {
    p->next = g_waiting;
    g_waiting = p;
}

static void remove_waiting(MMPlayer *p) {
    MMPlayer **cur = &g_waiting;
    while (*cur) {
        if (*cur == p) {
            *cur = p->next;
            p->next = NULL;
            return;
        }
        cur = &((*cur)->next);
    }
}

static void make_match_id(char *buf, size_t size) {
    snprintf(buf, size, "M%06d", g_next_match_id++);
}

static MMPlayer *find_match(MMPlayer *me) {
    MMPlayer *p = g_waiting;
    while (p) {
        if (p != me && strcmp(p->time_mode, me->time_mode) == 0) {
            int diff = me->elo - p->elo;
            if (diff < 0) diff = -diff;
            if (diff <= 300) return p;
        }
        p = p->next;
    }
    return NULL;
}

/*
 * MMJOIN <user_id> <elo> <time_mode>
 * -> QUEUED
 * -> MATCHED <match_id> <white_id> <black_id> <my_color>
 */
int handle_mmjoin(const char *payload, char *out, size_t out_size) {
    char user_id[64];
    char time_mode[16];
    int elo = 0;

    if (sscanf(payload, "%63s %d %15s", user_id, &elo, time_mode) != 3) {
        snprintf(out, out_size, "ERROR invalid_payload\n");
        return 0;
    }

    MMPlayer *p = find_player(user_id);
    if (!p) {
        p = calloc(1, sizeof(MMPlayer));
        strncpy(p->id, user_id, sizeof(p->id) - 1);
        add_player(p);
    }

    p->elo = elo;
    strncpy(p->time_mode, time_mode, sizeof(p->time_mode) - 1);
    p->in_match = 0;
    p->match_id[0] = '\0';
    p->opponent[0] = '\0';
    p->color[0] = '\0';

    add_waiting(p);

    MMPlayer *cand = find_match(p);
    if (cand) {
        char match_id[64];
        make_match_id(match_id, sizeof(match_id));

        p->in_match = 1;
        cand->in_match = 1;

        strncpy(p->match_id, match_id, sizeof(p->match_id) - 1);
        strncpy(cand->match_id, match_id, sizeof(cand->match_id) - 1);

        strncpy(p->opponent, cand->id, sizeof(p->opponent) - 1);
        strncpy(cand->opponent, p->id, sizeof(cand->opponent) - 1);

        if (rand() % 2 == 0) {
            strcpy(p->color, "white");
            strcpy(cand->color, "black");
        } else {
            strcpy(p->color, "black");
            strcpy(cand->color, "white");
        }

        remove_waiting(p);
        remove_waiting(cand);

        const char *white_id;
        const char *black_id;
        if (strcmp(p->color, "white") == 0) {
            white_id = p->id;
            black_id = p->opponent;
        } else {
            white_id = p->opponent;
            black_id = p->id;
        }

        snprintf(out, out_size, "MATCHED %s %s %s %s\n",
                 p->match_id, white_id, black_id, p->color);
        return 0;
    }

    snprintf(out, out_size, "QUEUED\n");
    return 0;
}

/*
 * MMSTATUS <user_id>
 * -> WAITING
 * -> MATCHED <match_id> <white_id> <black_id> <my_color>
 * -> NOTFOUND
 */
int handle_mmstatus(const char *payload, char *out, size_t out_size) {
    char user_id[64];
    if (sscanf(payload, "%63s", user_id) != 1) {
        snprintf(out, out_size, "ERROR invalid_payload\n");
        return 0;
    }

    MMPlayer *p = find_player(user_id);
    if (!p) {
        snprintf(out, out_size, "NOTFOUND\n");
        return 0;
    }

    if (!p->in_match) {
        snprintf(out, out_size, "WAITING\n");
        return 0;
    }

    const char *white_id;
    const char *black_id;
    if (strcmp(p->color, "white") == 0) {
        white_id = p->id;
        black_id = p->opponent;
    } else {
        white_id = p->opponent;
        black_id = p->id;
    }

    snprintf(out, out_size, "MATCHED %s %s %s %s\n",
             p->match_id, white_id, black_id, p->color);
    return 0;
}

/*
 * MMCANCEL <user_id>
 * -> CANCELED
 * -> NOTFOUND
 */
int handle_mmcancel(const char *payload, char *out, size_t out_size) {
    char user_id[64];
    if (sscanf(payload, "%63s", user_id) != 1) {
        snprintf(out, out_size, "ERROR invalid_payload\n");
        return 0;
    }

    MMPlayer *p = g_waiting;
    MMPlayer *prev = NULL;

    while (p) {
        if (strcmp(p->id, user_id) == 0) {
            if (prev) prev->next = p->next;
            else g_waiting = p->next;
            p->next = NULL;

            snprintf(out, out_size, "CANCELED\n");
            return 0;
        }
        prev = p;
        p = p->next;
    }

    snprintf(out, out_size, "NOTFOUND\n");
    return 0;
}
