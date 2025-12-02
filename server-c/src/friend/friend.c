// friend.c - Friend system handlers
#include "friend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

void handle_friend_request(ClientSession *session, int num_params, char *param1, char *param2, PGconn *db) {
    // FRIEND_REQUEST|user_id|friend_id
    if (num_params >= 3) {
        int user_id = atoi(param1);
        int friend_id = atoi(param2);
        if (user_id == friend_id) {
            char error[] = "ERROR|Cannot add yourself as friend\n";
            send(session->socket_fd, error, strlen(error), 0);
            return;
        }
        // Check if already exists
        char check_query[256];
        snprintf(check_query, sizeof(check_query),
            "SELECT status FROM friendship WHERE user_id=%d AND friend_id=%d;",
            user_id, friend_id);
        PGresult *check_res = PQexec(db, check_query);
        if (PQresultStatus(check_res) == PGRES_TUPLES_OK && PQntuples(check_res) > 0) {
            char error[] = "ERROR|Friend request already exists\n";
            send(session->socket_fd, error, strlen(error), 0);
            PQclear(check_res);
            return;
        }
        PQclear(check_res);
        // Insert new request
        char insert_query[256];
        snprintf(insert_query, sizeof(insert_query),
            "INSERT INTO friendship (user_id, friend_id, status) VALUES (%d, %d, 'pending');",
            user_id, friend_id);
        PGresult *insert_res = PQexec(db, insert_query);
        if (PQresultStatus(insert_res) == PGRES_COMMAND_OK) {
            char response[] = "FRIEND_REQUESTED\n";
            send(session->socket_fd, response, strlen(response), 0);
        } else {
            char error[] = "ERROR|Failed to send friend request\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        PQclear(insert_res);
    }
}

void handle_friend_accept(ClientSession *session, int num_params, char *param1, char *param2, PGconn *db) {
    // FRIEND_ACCEPT|user_id|friend_id
    if (num_params >= 3) {
        int user_id = atoi(param1);
        int friend_id = atoi(param2);
        // Update status
        char update_query[256];
        snprintf(update_query, sizeof(update_query),
            "UPDATE friendship SET status='accepted', responded_at=NOW() WHERE user_id=%d AND friend_id=%d AND status='pending';",
            friend_id, user_id);
        PGresult *update_res = PQexec(db, update_query);
        if (PQresultStatus(update_res) == PGRES_COMMAND_OK && atoi(PQcmdTuples(update_res)) > 0) {
            // Insert reverse relation for easy query
            char insert_query[256];
            snprintf(insert_query, sizeof(insert_query),
                "INSERT INTO friendship (user_id, friend_id, status) VALUES (%d, %d, 'accepted') ON CONFLICT DO NOTHING;",
                user_id, friend_id);
            PQexec(db, insert_query);
            char response[] = "FRIEND_ACCEPTED\n";
            send(session->socket_fd, response, strlen(response), 0);
        } else {
            char error[] = "ERROR|No pending request found\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        PQclear(update_res);
    }
}

void handle_friend_decline(ClientSession *session, int num_params, char *param1, char *param2, PGconn *db) {
    // FRIEND_DECLINE|user_id|friend_id
    if (num_params >= 3) {
        int user_id = atoi(param1);
        int friend_id = atoi(param2);
        char update_query[256];
        snprintf(update_query, sizeof(update_query),
            "UPDATE friendship SET status='declined', responded_at=NOW() WHERE user_id=%d AND friend_id=%d AND status='pending';",
            friend_id, user_id);
        PGresult *update_res = PQexec(db, update_query);
        if (PQresultStatus(update_res) == PGRES_COMMAND_OK && atoi(PQcmdTuples(update_res)) > 0) {
            char response[] = "FRIEND_DECLINED\n";
            send(session->socket_fd, response, strlen(response), 0);
        } else {
            char error[] = "ERROR|No pending request found\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        PQclear(update_res);
    }
}

void handle_friend_list(ClientSession *session, int num_params, char *param1, PGconn *db) {
    // FRIEND_LIST|user_id
    if (num_params >= 2) {
        int user_id = atoi(param1);
        char query[256];
        snprintf(query, sizeof(query),
            "SELECT friend_id FROM friendship WHERE user_id=%d AND status='accepted';",
            user_id);
        PGresult *res = PQexec(db, query);
        if (PQresultStatus(res) == PGRES_TUPLES_OK) {
            char response[1024] = "FRIEND_LIST|";
            for (int i = 0; i < PQntuples(res); i++) {
                strcat(response, PQgetvalue(res, i, 0));
                if (i < PQntuples(res) - 1) strcat(response, ",");
            }
            strcat(response, "\n");
            send(session->socket_fd, response, strlen(response), 0);
        } else {
            char error[] = "ERROR|Failed to get friend list\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        PQclear(res);
    }
}

void handle_friend_requests(ClientSession *session, int num_params, char *param1, PGconn *db) {
    // FRIEND_REQUESTS|user_id
    if (num_params >= 2) {
        int user_id = atoi(param1);
        char query[256];
        snprintf(query, sizeof(query),
            "SELECT user_id FROM friendship WHERE friend_id=%d AND status='pending';",
            user_id);
        PGresult *res = PQexec(db, query);
        if (PQresultStatus(res) == PGRES_TUPLES_OK) {
            char response[1024] = "FRIEND_REQUESTS|";
            for (int i = 0; i < PQntuples(res); i++) {
                strcat(response, PQgetvalue(res, i, 0));
                if (i < PQntuples(res) - 1) strcat(response, ",");
            }
            strcat(response, "\n");
            send(session->socket_fd, response, strlen(response), 0);
        } else {
            char error[] = "ERROR|Failed to get friend requests\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        PQclear(res);
    }
}
