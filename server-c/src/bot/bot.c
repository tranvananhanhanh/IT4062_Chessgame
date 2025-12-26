// bot.c - Bot game handlers
#include "bot.h"
#include "game.h"
#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

extern GameManager game_manager;

void handle_mode_bot(ClientSession *session, char *user_id, char *difficulty, PGconn *db) {
    // MODE_BOT|user_id|difficulty
    int user_id_int = atoi(user_id);
    printf("[Bot] Initializing bot match for user %d (difficulty: %s)\n", user_id_int, difficulty);

    // Create a match against bot (black player is bot)
    Player white = {user_id_int, session->socket_fd, COLOR_WHITE, 1, ""};
    snprintf(white.username, sizeof(white.username), "Player_%d", user_id_int);

    Player black = {0, -1, COLOR_BLACK, 1, "AI_Bot"};
    strcpy(black.username, "AI_Bot");

    GameMatch *match = game_manager_create_bot_match(&game_manager, white, black, db, difficulty);

    if (match != NULL) {
        session->current_match = match;
        session->user_id = user_id_int;
        strcpy(session->username, white.username);
        session->is_bot_mode = 1;
        char response[512];
        sprintf(response, "BOT_MATCH_CREATED|%d|%s\n", match->match_id, match->board.fen);
        send(session->socket_fd, response, strlen(response), 0);
        printf("[Bot] Bot match %d created successfully (difficulty: %s)\n", match->match_id, difficulty);
    } else {
        char error[] = "ERROR|Failed to create bot match\n";
        send(session->socket_fd, error, strlen(error), 0);
        printf("[Bot] Failed to create bot match\n");
    }
}

// Helper function to communicate with Python chess bot
char* call_python_bot(const char *fen, const char *player_move, const char *difficulty, char *bot_move_out, size_t bot_move_size) {
    // Connect to Python bot server (default port 5001)
    const char *bot_host = getenv("BOT_HOST") ? getenv("BOT_HOST") : "127.0.0.1";
    int bot_port = getenv("BOT_PORT") ? atoi(getenv("BOT_PORT")) : 5001;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[Bot] Socket creation failed");
        return NULL;
    }

    struct sockaddr_in bot_addr;
    memset(&bot_addr, 0, sizeof(bot_addr));
    bot_addr.sin_family = AF_INET;
    bot_addr.sin_port = htons(bot_port);

    if (inet_pton(AF_INET, bot_host, &bot_addr.sin_addr) <= 0) {
        perror("[Bot] Invalid address");
        close(sock);
        return NULL;
    }

    if (connect(sock, (struct sockaddr *)&bot_addr, sizeof(bot_addr)) < 0) {
        perror("[Bot] Connection to bot server failed");
        close(sock);
        return NULL;
    }

    // Send: "fen|player_move|difficulty\n"
    char request[2048];
    snprintf(request, sizeof(request), "%s|%s|%s\n", fen, player_move, difficulty);

    if (send(sock, request, strlen(request), 0) < 0) {
        perror("[Bot] Send to bot failed");
        close(sock);
        return NULL;
    }

    // Receive: "fen_after_player|bot_move|fen_after_bot\n"
    char response[4096];
    ssize_t bytes_read = recv(sock, response, sizeof(response) - 1, 0);
    close(sock);

    if (bytes_read <= 0) {
        fprintf(stderr, "[Bot] No response from bot server\n");
        return NULL;
    }

    response[bytes_read] = '\0';

    // Remove trailing newline
    char *newline = strchr(response, '\n');
    if (newline) *newline = '\0';

    printf("[Bot] Received from Python bot: %s\n", response);

    // Parse "fen_after_player|bot_move|fen_after_bot"
    char *p1 = strchr(response, '|');
    if (!p1) {
        fprintf(stderr, "[Bot] Invalid response format (missing first |)\n");
        return NULL;
    }
    *p1 = '\0';
    char *fen_after_player = response;

    char *p2 = strchr(p1 + 1, '|');
    if (!p2) {
        fprintf(stderr, "[Bot] Invalid response format (missing second |)\n");
        return NULL;
    }
    *p2 = '\0';
    char *bot_move = p1 + 1;
    char *fen_after_bot = p2 + 1;

    // Copy bot move to output buffer
    strncpy(bot_move_out, bot_move, bot_move_size);
    bot_move_out[bot_move_size - 1] = '\0';

    // Return combined FEN string "fen_after_player|fen_after_bot"
    char *result = (char*)malloc(strlen(fen_after_player) + strlen(fen_after_bot) + 2);
    sprintf(result, "%s|%s", fen_after_player, fen_after_bot);

    return result;
}

void handle_bot_move(ClientSession *session, int num_params, char *param1, char *param2, char *param3, PGconn *db) {
    // BOT_MOVE|match_id|player_move|difficulty
    if (num_params >= 4) {
        int match_id = atoi(param1);
        char *player_move = param2;
        char *difficulty = param3;
        GameMatch *match = game_manager_find_match(&game_manager, match_id);
        if (match == NULL) {
            char error[] = "ERROR|Match not found\n";
            send(session->socket_fd, error, strlen(error), 0);
            return;
        }
        pthread_mutex_lock(&match->lock);
        if (match->status != GAME_PLAYING) {
            char error[100];
            if (match->status == GAME_FINISHED) {
                snprintf(error, sizeof(error), "ERROR|Game has ended\n");
            } else if (match->status == GAME_PAUSED) {
                snprintf(error, sizeof(error), "ERROR|Game is paused\n");
            } else {
                snprintf(error, sizeof(error), "ERROR|Game is not in playing state\n");
            }
            send(session->socket_fd, error, strlen(error), 0);
            pthread_mutex_unlock(&match->lock);
            printf("[Bot Move] Rejected: match %d status=%d\n", match_id, match->status);
            return;
        }
        if (match->board.current_turn != COLOR_WHITE) {
            char error[] = "ERROR|Not your turn (bot is thinking)\n";
            send(session->socket_fd, error, strlen(error), 0);
            pthread_mutex_unlock(&match->lock);
            printf("[Bot Move] Rejected: not white's turn in match %d\n", match_id);
            return;
        }
        char current_fen[FEN_MAX_LENGTH];
        strcpy(current_fen, match->board.fen);
        printf("[Bot] Processing move %s on FEN: %s (difficulty: %s)\n", player_move, current_fen, difficulty);
        char bot_move[16] = {0};
        char *fen_combo = call_python_bot(current_fen, player_move, difficulty, bot_move, sizeof(bot_move));
        if (fen_combo == NULL || strncmp(bot_move, "ERROR", 5) == 0) {
            char error[] = "ERROR|Bot engine failed\n";
            send(session->socket_fd, error, strlen(error), 0);
            pthread_mutex_unlock(&match->lock);
            return;
        }
        char *sep = strchr(fen_combo, '|');
        if (!sep) {
            char error[] = "ERROR|Invalid bot response format\n";
            send(session->socket_fd, error, strlen(error), 0);
            free(fen_combo);
            pthread_mutex_unlock(&match->lock);
            return;
        }
        *sep = '\0';
        char *fen_after_player = fen_combo;
        char *fen_after_bot = sep + 1;
        Move player_move_struct;
        notation_to_coords(player_move, &player_move_struct.from_row, &player_move_struct.from_col);
        notation_to_coords(player_move + 2, &player_move_struct.to_row, &player_move_struct.to_col);
        player_move_struct.piece = match->board.board[player_move_struct.from_row][player_move_struct.from_col];
        player_move_struct.captured_piece = match->board.board[player_move_struct.to_row][player_move_struct.to_col];
        player_move_struct.is_castling = 0;
        player_move_struct.is_en_passant = 0;
        player_move_struct.is_promotion = 0;
        player_move_struct.promotion_piece = 0;
        execute_move(&match->board, &player_move_struct);
        if (strcmp(bot_move, "NOMOVE") != 0 && strcmp(bot_move, "NONE") != 0) {
            Move bot_move_struct;
            notation_to_coords(bot_move, &bot_move_struct.from_row, &bot_move_struct.from_col);
            notation_to_coords(bot_move + 2, &bot_move_struct.to_row, &bot_move_struct.to_col);
            bot_move_struct.piece = match->board.board[bot_move_struct.from_row][bot_move_struct.from_col];
            bot_move_struct.captured_piece = match->board.board[bot_move_struct.to_row][bot_move_struct.to_col];
            bot_move_struct.is_castling = 0;
            bot_move_struct.is_en_passant = 0;
            bot_move_struct.is_promotion = 0;
            bot_move_struct.promotion_piece = 0;
            execute_move(&match->board, &bot_move_struct);
            strcpy(match->board.fen, fen_after_bot);
        } else {
            strcpy(match->board.fen, fen_after_player);
            printf("[Bot] Bot returned NOMOVE - checking if checkmate or stalemate\n");
            if (is_king_in_check(&match->board, COLOR_BLACK)) {
                printf("[Bot] Black is in checkmate! White wins!\n");
                match->status = GAME_FINISHED;
                match->result = RESULT_WHITE_WIN;
                match->winner_id = match->white_player.user_id;
                match->end_time = time(NULL);
                history_update_match_result(db, match->match_id, "white_win", match->winner_id);
                timer_manager_stop_timer(&game_manager.timer_manager, match->match_id);
                char msg[BUFFER_SIZE];
                sprintf(msg, "GAME_END|checkmate|winner:%d\n", match->winner_id);
                broadcast_to_match(match, msg, -1);
                sprintf(msg, "BOT_GAME_END|white_win|winner:%d\n", match->white_player.user_id);
                send_to_client(match->white_player.socket_fd, msg);
            } else {
                printf("[Bot] Stalemate! Draw!\n");
                match->status = GAME_FINISHED;
                match->result = RESULT_DRAW;
                match->end_time = time(NULL);
                history_update_match_result(db, match->match_id, "draw", 0);
                timer_manager_stop_timer(&game_manager.timer_manager, match->match_id);
                char msg[BUFFER_SIZE];
                sprintf(msg, "GAME_END|stalemate|draw\n");
                broadcast_to_match(match, msg, -1);
                sprintf(msg, "BOT_GAME_END|draw\n");
                send_to_client(match->white_player.socket_fd, msg);
            }
        }
        history_save_move(db, match_id, session->user_id, player_move, fen_after_player);
        if (strcmp(bot_move, "NONE") != 0) {
            history_save_bot_move(db, match_id, bot_move, fen_after_bot);
        }
        game_match_check_end_condition(match, db);
        char status[32] = "IN_GAME";
        printf("[DEBUG] After game_match_check_end_condition: match->status=%d (0=WAITING,1=PLAYING,2=FINISHED)\n", match->status);
        printf("[DEBUG] match->result=%d (0=NONE,1=WHITE_WIN,2=BLACK_WIN,3=DRAW)\n", match->result);
        if (match->status == GAME_FINISHED) {
            if (match->result == RESULT_WHITE_WIN) {
                strcpy(status, "WHITE_WIN");
            } else if (match->result == RESULT_BLACK_WIN) {
                strcpy(status, "BLACK_WIN");
            } else if (match->result == RESULT_DRAW) {
                strcpy(status, "DRAW");
            }
        }
        printf("[DEBUG] Status string being sent: '%s'\n", status);
        char response[2048];
        snprintf(response, sizeof(response),
                "BOT_MOVE_RESULT|%s|%s|%s|%s\n",
                fen_after_player, bot_move, fen_after_bot, status);
        send(session->socket_fd, response, strlen(response), 0);
        printf("[Bot] Move processed: player=%s, bot=%s, status=%s\n", 
               player_move, bot_move, status);
        free(fen_combo);
        pthread_mutex_unlock(&match->lock);
    } else if (num_params >= 2 && session->current_match != NULL) {
        char *player_move = param1;
        pthread_mutex_lock(&session->current_match->lock);
        char current_fen[FEN_MAX_LENGTH];
        strcpy(current_fen, session->current_match->board.fen);
        char bot_move[16] = {0};
        char *fen_combo = call_python_bot(current_fen, player_move, session->current_match->bot_difficulty, bot_move, sizeof(bot_move));
        if (fen_combo) {
            char *sep = strchr(fen_combo, '|');
            if (sep) {
                *sep = '\0';
                strcpy(session->current_match->board.fen, sep + 1);
                game_match_check_end_condition(session->current_match, db);
                char response[512];
                snprintf(response, sizeof(response),
                        "BOT_MOVE_RESULT|%s|%s\n", bot_move, sep + 1);
                send(session->socket_fd, response, strlen(response), 0);
            }
            free(fen_combo);
        } else {
            char error[] = "ERROR|Bot engine failed\n";
            send(session->socket_fd, error, strlen(error), 0);
        }
        pthread_mutex_unlock(&session->current_match->lock);
    } else {
        char error[] = "ERROR|Invalid BOT_MOVE command format\n";
        send(session->socket_fd, error, strlen(error), 0);
    }
}
