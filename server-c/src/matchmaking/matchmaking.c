#include "matchmaking.h"
#include "client_session.h"
#include "database.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>

typedef struct MMPlayer {
    char id[64];        // username
    int user_id;        // user id from database
    int elo;
    char time_mode[16]; // BLITZ / RAPID...

    int in_match;       // 0: chưa, 1: đã ghép
    char match_id[64];  // M000001
    char opponent[64];  // username đối thủ
    int opponent_id;    // opponent user_id
    char color[8];      // "white" / "black"
    
    ClientSession *session;  // Session để gửi thông báo

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
    // Parse payload: <user_id> <elo> <time_mode>
    char user_id[64], time_mode[16];
    int elo;
    if (sscanf(payload, "%63s %d %15s", user_id, &elo, time_mode) != 3) {
        snprintf(out, out_size, "ERROR|Invalid MMJOIN payload\n");
        return 0;
    }
    MMPlayer *me = find_player(user_id);
    if (!me) {
        me = (MMPlayer*)calloc(1, sizeof(MMPlayer));
        strncpy(me->id, user_id, sizeof(me->id)-1);
        me->elo = elo;
        strncpy(me->time_mode, time_mode, sizeof(me->time_mode)-1);
        me->in_match = 0;
        add_player(me);
    } else {
        me->elo = elo;
        strncpy(me->time_mode, time_mode, sizeof(me->time_mode)-1);
        me->in_match = 0;
        memset(me->match_id, 0, sizeof(me->match_id));
        memset(me->opponent, 0, sizeof(me->opponent));
        memset(me->color, 0, sizeof(me->color));
    }
    // Nếu đã trong hàng chờ thì thôi
    MMPlayer *p = g_waiting;
    while (p) {
        if (p == me) {
            snprintf(out, out_size, "QUEUED\n");
            return 0;
        }
        p = p->next;
    }
    // Tìm đối thủ phù hợp
    MMPlayer *opp = find_match(me);
    if (opp) {
        // Ghép trận
        char match_id[64];
        make_match_id(match_id, sizeof(match_id));
        strncpy(me->match_id, match_id, sizeof(me->match_id)-1);
        strncpy(opp->match_id, match_id, sizeof(opp->match_id)-1);
        me->in_match = opp->in_match = 1;
        // Sửa logic gán opponent đúng
        strncpy(me->opponent, opp->id, sizeof(me->opponent)-1);
        strncpy(opp->opponent, me->id, sizeof(opp->opponent)-1);
        // Random màu
        if (rand() % 2) {
            strcpy(me->color, "white");
            strcpy(opp->color, "black");
        } else {
            strcpy(me->color, "black");
            strcpy(opp->color, "white");
        }
        remove_waiting(opp);
        // Trả về đúng thứ tự: match_id, white_id, black_id, my_color
        char white_id[64], black_id[64];
        if (strcmp(me->color, "white") == 0) {
            strncpy(white_id, me->id, sizeof(white_id)-1);
            strncpy(black_id, opp->id, sizeof(black_id)-1);
        } else {
            strncpy(white_id, opp->id, sizeof(white_id)-1);
            strncpy(black_id, me->id, sizeof(black_id)-1);
        }
        snprintf(out, out_size, "MATCHED %s %s %s %s\n", match_id, white_id, black_id, me->color);
        return 1;
    } else {
        // Thêm vào hàng chờ
        add_waiting(me);
        snprintf(out, out_size, "QUEUED\n");
        return 0;
    }
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
        snprintf(out, out_size, "ERROR|Invalid MMSTATUS payload\n");
        return 0;
    }
    MMPlayer *me = find_player(user_id);
    if (!me) {
        snprintf(out, out_size, "NOTFOUND\n");
        return 0;
    }
    if (me->in_match) {
        // Trả về đúng thứ tự: match_id, white_id, black_id, my_color
        MMPlayer *opp = find_player(me->opponent);
        char white_id[64], black_id[64];
        if (opp) {
            if (strcmp(me->color, "white") == 0) {
                strncpy(white_id, me->id, sizeof(white_id)-1);
                strncpy(black_id, opp->id, sizeof(black_id)-1);
            } else {
                strncpy(white_id, opp->id, sizeof(white_id)-1);
                strncpy(black_id, me->id, sizeof(black_id)-1);
            }
            snprintf(out, out_size, "MATCHED %s %s %s %s\n", me->match_id, white_id, black_id, me->color);
        } else {
            // Nếu không tìm thấy đối thủ, fallback về opponent cũ
            snprintf(out, out_size, "MATCHED %s %s %s %s\n", me->match_id, me->id, me->opponent, me->color);
        }
        return 1;
    } else {
        snprintf(out, out_size, "WAITING\n");
        return 0;
    }
}

/*
 * MMCANCEL <user_id>
 * -> CANCELED
 * -> NOTFOUND
 */
int handle_mmcancel(const char *payload, char *out, size_t out_size) {
    char user_id[64];
    if (sscanf(payload, "%63s", user_id) != 1) {
        snprintf(out, out_size, "ERROR|Invalid MMCANCEL payload\n");
        return 0;
    }
    MMPlayer *me = find_player(user_id);
    if (!me) {
        snprintf(out, out_size, "NOTFOUND\n");
        return 0;
    }
    // Nếu đang trong hàng chờ thì xóa khỏi hàng chờ
    MMPlayer *p = g_waiting;
    while (p) {
        if (p == me) {
            remove_waiting(me);
            snprintf(out, out_size, "CANCELED\n");
            return 1;
        }
        p = p->next;
    }
    snprintf(out, out_size, "NOTFOUND\n");
    return 0;
}

// ===== New simple handlers for JOIN_MATCHMAKING / LEAVE_MATCHMAKING =====

static void update_user_status(PGconn *db, int user_id, const char *status) {
    if (!db) return;
    char query[256];
    snprintf(query, sizeof(query), 
        "UPDATE users SET state = '%s' WHERE user_id = %d", status, user_id);
    PGresult *res = PQexec(db, query);
    PQclear(res);
}

void handle_join_matchmaking(ClientSession *session, char *user_id_str, PGconn *db) {
    int user_id = atoi(user_id_str);
    
    // Tạo hoặc tìm player
    MMPlayer *me = find_player(session->username);
    if (!me) {
        me = (MMPlayer*)calloc(1, sizeof(MMPlayer));
        strncpy(me->id, session->username, sizeof(me->id)-1);
        me->user_id = user_id;
        me->elo = 1000; // Default ELO
        strcpy(me->time_mode, "NORMAL");
        me->in_match = 0;
        me->session = session;
        add_player(me);
    } else {
        me->session = session;
        me->user_id = user_id;
        me->in_match = 0;
    }
    
    // Kiểm tra xem đã trong hàng chờ chưa
    MMPlayer *p = g_waiting;
    while (p) {
        if (p == me) {
            char resp[] = "MATCHMAKING_QUEUED\n";
            send(session->socket_fd, resp, strlen(resp), 0);
            return;
        }
        p = p->next;
    }
    
    // Tìm đối thủ
    MMPlayer *opp = find_match(me);
    if (opp && opp->session) {
        // Ghép trận thành công
        char match_id[64];
        make_match_id(match_id, sizeof(match_id));
        
        strncpy(me->match_id, match_id, sizeof(me->match_id)-1);
        strncpy(opp->match_id, match_id, sizeof(opp->match_id)-1);
        me->in_match = opp->in_match = 1;
        
        strncpy(me->opponent, opp->id, sizeof(me->opponent)-1);
        strncpy(opp->opponent, me->id, sizeof(opp->opponent)-1);
        me->opponent_id = opp->user_id;
        opp->opponent_id = me->user_id;
        
        // Random màu
        if (rand() % 2) {
            strcpy(me->color, "white");
            strcpy(opp->color, "black");
        } else {
            strcpy(me->color, "black");
            strcpy(opp->color, "white");
        }
        
        remove_waiting(opp);
        
        // Cập nhật status trong database
        update_user_status(db, me->user_id, "ingame");
        update_user_status(db, opp->user_id, "ingame");
        
        // Tạo match trong game manager
        Player white_player, black_player;
        if (strcmp(me->color, "white") == 0) {
            white_player.user_id = me->user_id;
            white_player.socket_fd = me->session->socket_fd;
            white_player.color = COLOR_WHITE;
            white_player.is_online = 1;
            strncpy(white_player.username, me->id, sizeof(white_player.username)-1);
            
            black_player.user_id = opp->user_id;
            black_player.socket_fd = opp->session->socket_fd;
            black_player.color = COLOR_BLACK;
            black_player.is_online = 1;
            strncpy(black_player.username, opp->id, sizeof(black_player.username)-1);
        } else {
            white_player.user_id = opp->user_id;
            white_player.socket_fd = opp->session->socket_fd;
            white_player.color = COLOR_WHITE;
            white_player.is_online = 1;
            strncpy(white_player.username, opp->id, sizeof(white_player.username)-1);
            
            black_player.user_id = me->user_id;
            black_player.socket_fd = me->session->socket_fd;
            black_player.color = COLOR_BLACK;
            black_player.is_online = 1;
            strncpy(black_player.username, me->id, sizeof(black_player.username)-1);
        }
        
        // Create match using game manager
        extern GameManager game_manager;
        GameMatch *match = game_manager_create_match(&game_manager, white_player, black_player, db);
        
        if (match) {
            // Set match to PLAYING immediately
            match->status = GAME_PLAYING;
            
            // Update both sessions
            me->session->current_match = match;
            opp->session->current_match = match;
            
            // Gửi thông báo cho cả 2 người chơi với match_id thật từ database
            char msg_me[256], msg_opp[256];
            snprintf(msg_me, sizeof(msg_me), "MATCH_FOUND|%d|%s|%s\n", 
                     match->match_id, opp->id, me->color);
            snprintf(msg_opp, sizeof(msg_opp), "MATCH_FOUND|%d|%s|%s\n", 
                     match->match_id, me->id, opp->color);
            
            send(me->session->socket_fd, msg_me, strlen(msg_me), 0);
            send(opp->session->socket_fd, msg_opp, strlen(msg_opp), 0);
            
            printf("[Matchmaking] Match %d created: %s vs %s\n", 
                   match->match_id, me->id, opp->id);
        } else {
            // Failed to create match
            char error[] = "ERROR|Failed to create match\n";
            send(me->session->socket_fd, error, strlen(error), 0);
            send(opp->session->socket_fd, error, strlen(error), 0);
        }
    } else {
        // Thêm vào hàng chờ
        add_waiting(me);
        char resp[] = "MATCHMAKING_QUEUED\n";
        send(session->socket_fd, resp, strlen(resp), 0);
        printf("[Matchmaking] Player %s added to queue\n", me->id);
    }
}

void handle_leave_matchmaking(ClientSession *session, char *user_id_str, PGconn *db) {
    MMPlayer *me = find_player(session->username);
    if (!me) {
        char resp[] = "ERROR|Not in matchmaking\n";
        send(session->socket_fd, resp, strlen(resp), 0);
        return;
    }
    
    // Xóa khỏi hàng chờ
    MMPlayer *p = g_waiting;
    while (p) {
        if (p == me) {
            remove_waiting(me);
            char resp[] = "MATCHMAKING_LEFT\n";
            send(session->socket_fd, resp, strlen(resp), 0);
            printf("[Matchmaking] Player %s left queue\n", me->id);
            return;
        }
        p = p->next;
    }
    
    char resp[] = "ERROR|Not in queue\n";
    send(session->socket_fd, resp, strlen(resp), 0);
}
