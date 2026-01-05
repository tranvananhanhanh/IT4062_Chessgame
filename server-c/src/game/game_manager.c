#include "game.h"
#include "history.h"
#include "timer.h"
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// External global variables
extern PGconn *db_conn;
extern GameManager game_manager;

void game_manager_init(GameManager *manager) {
    manager->match_count = 0;
    timer_manager_init(&manager->timer_manager);
    pthread_mutex_init(&manager->lock, NULL);
    for (int i = 0; i < MAX_MATCHES; i++) {
        manager->matches[i] = NULL;
    }
}

GameMatch* game_manager_create_match(GameManager *manager, Player white, Player black, PGconn *db) {
    pthread_mutex_lock(&manager->lock);
    
    if (manager->match_count >= MAX_MATCHES) {
        pthread_mutex_unlock(&manager->lock);
        return NULL;
    }
    
    // Create match in database first (status='waiting')
    int match_id = history_create_match(db, white.user_id, black.user_id, "pvp");
    if (match_id < 0) {
        pthread_mutex_unlock(&manager->lock);
        return NULL;
    }
    
    // Allocate and initialize match
    GameMatch *match = (GameMatch*)malloc(sizeof(GameMatch));
    match->match_id = match_id;
    match->status = GAME_WAITING;  
    match->white_player = white;
    match->black_player = black;
    match->start_time = time(NULL);
    match->end_time = 0;
    match->result = RESULT_NONE;
    match->winner_id = 0;
    match->draw_requester_id = 0;
    match->rematch_id = 0;  

    // Initialize timers: 10 minutes each
    match->white_time_remaining = 10 * 60;
    match->black_time_remaining = 10 * 60;
    match->last_move_time = time(NULL);
    
    chess_board_init(&match->board);
    pthread_mutex_init(&match->lock, NULL);
    
    // Add to manager
    manager->matches[manager->match_count] = match;
    manager->match_count++;
    
    // Create timer for the match (30 minutes per player)
    timer_manager_create_timer(&manager->timer_manager, match_id, 30, game_timeout_callback);
    
    pthread_mutex_unlock(&manager->lock);
    
    printf("[Game Manager] Created match %d: %s vs %s\n", 
           match_id, white.username, black.username);
    
    return match;
}

GameMatch* game_manager_create_bot_match(GameManager *manager, Player white, Player black, PGconn *db, const char *difficulty) {
    pthread_mutex_lock(&manager->lock);
    
    if (manager->match_count >= MAX_MATCHES) {
        pthread_mutex_unlock(&manager->lock);
        return NULL;
    }
    
    // Create bot match in database (bot player will have NULL user_id)
    int match_id = history_create_bot_match(db, white.user_id, "bot");
    if (match_id < 0) {
        pthread_mutex_unlock(&manager->lock);
        return NULL;
    }
    
    // Allocate and initialize match
    GameMatch *match = (GameMatch*)malloc(sizeof(GameMatch));
    match->match_id = match_id;
    match->status = GAME_PLAYING;
    match->white_player = white;
    match->black_player = black;
    match->start_time = time(NULL);
    match->end_time = 0;
    match->result = RESULT_NONE;
    match->winner_id = 0;
    match->draw_requester_id = 0;
    
    match->rematch_id = 0;  // ✅ Initialize rematch_id
    
    chess_board_init(&match->board);
    pthread_mutex_init(&match->lock, NULL);
    strncpy(match->bot_difficulty, difficulty, sizeof(match->bot_difficulty)-1);
    match->bot_difficulty[sizeof(match->bot_difficulty)-1] = '\0';
    
    // Add to manager
    manager->matches[manager->match_count] = match;
    manager->match_count++;
    
    // Create timer for the match (30 minutes per player)
    timer_manager_create_timer(&manager->timer_manager, match_id, 30, game_timeout_callback);
    
    pthread_mutex_unlock(&manager->lock);
    
    printf("[Game Manager] Created bot match %d: %s vs %s (difficulty: %s)\n", 
           match_id, white.username, black.username, difficulty);
    
    return match;
}

GameMatch* game_manager_find_match(GameManager *manager, int match_id) {
    pthread_mutex_lock(&manager->lock);
    
    for (int i = 0; i < manager->match_count; i++) {
        if (manager->matches[i] && manager->matches[i]->match_id == match_id) {
            GameMatch *match = manager->matches[i];
            pthread_mutex_unlock(&manager->lock);
            return match;
        }
    }
    
    pthread_mutex_unlock(&manager->lock);
    return NULL;
}

GameMatch* game_manager_find_match_by_player(GameManager *manager, int socket_fd) {
    pthread_mutex_lock(&manager->lock);
    
    for (int i = 0; i < manager->match_count; i++) {
        GameMatch *match = manager->matches[i];
        if (match && (match->white_player.socket_fd == socket_fd || 
                     match->black_player.socket_fd == socket_fd)) {
            pthread_mutex_unlock(&manager->lock);
            return match;
        }
    }
    
    pthread_mutex_unlock(&manager->lock);
    return NULL;
}

void game_manager_remove_match(GameManager *manager, int match_id) {
    pthread_mutex_lock(&manager->lock);
    
    for (int i = 0; i < manager->match_count; i++) {
        if (manager->matches[i] && manager->matches[i]->match_id == match_id) {
            // Stop timer for this match
            timer_manager_stop_timer(&manager->timer_manager, match_id);
            
            pthread_mutex_destroy(&manager->matches[i]->lock);
            free(manager->matches[i]);
            
            // Shift remaining matches
            for (int j = i; j < manager->match_count - 1; j++) {
                manager->matches[j] = manager->matches[j + 1];
            }
            manager->matches[manager->match_count - 1] = NULL;
            manager->match_count--;
            
            printf("[Game Manager] Removed match %d\n", match_id);
            break;
        }
    }
    
    pthread_mutex_unlock(&manager->lock);
}

static void force_end_game(GameMatch *match,
                           PGconn *db,
                           int winner_id,
                           const char *result_str)
{
    match->status = GAME_FINISHED;
    match->end_time = time(NULL);
    match->winner_id = winner_id;

    history_update_match_result(db,
                                match->match_id,
                                result_str,
                                winner_id);

    timer_manager_stop_timer(&game_manager.timer_manager,
                             match->match_id);

    char msg[BUFFER_SIZE];
    snprintf(msg, sizeof(msg),
             "GAME_END|invalid_state|winner:%d\n",
             winner_id);

    broadcast_to_match(match, msg, -1);
}

int game_match_make_move(GameMatch *match,
                         int player_socket_fd,
                         int player_id,
                         const char *from,
                         const char *to,
                         PGconn *db)
{
    pthread_mutex_lock(&match->lock);

    /* 1️⃣ CHECK GAME STATE */
    if (match->status != GAME_PLAYING) {
        char error[100];
        if (match->status == GAME_FINISHED)
            snprintf(error, sizeof(error), "ERROR|Game has already ended\n");
        else if (match->status == GAME_PAUSED)
            snprintf(error, sizeof(error), "ERROR|Game is paused\n");
        else if (match->status == GAME_WAITING)
            snprintf(error, sizeof(error), "ERROR|Waiting for opponent\n");
        else
            snprintf(error, sizeof(error), "ERROR|Invalid game state\n");

        send_to_client(player_socket_fd, error);
        pthread_mutex_unlock(&match->lock);
        return 0;
    }

    /* 2️⃣ IDENTIFY PLAYER */
    Player *current_player = NULL;
    Player *opponent = NULL;

    if (player_id > 0) {
        if (match->white_player.user_id == player_id) {
            current_player = &match->white_player;
            opponent = &match->black_player;
        } else if (match->black_player.user_id == player_id) {
            current_player = &match->black_player;
            opponent = &match->white_player;
        } else {
            pthread_mutex_unlock(&match->lock);
            return 0;
        }
    } else {
        if (match->white_player.socket_fd == player_socket_fd) {
            current_player = &match->white_player;
            opponent = &match->black_player;
        } else if (match->black_player.socket_fd == player_socket_fd) {
            current_player = &match->black_player;
            opponent = &match->white_player;
        } else {
            pthread_mutex_unlock(&match->lock);
            return 0;
        }
    }

    /* 3️⃣ CHECK TURN */
    if ((match->board.current_turn == COLOR_WHITE &&
         current_player->color != COLOR_WHITE) ||
        (match->board.current_turn == COLOR_BLACK &&
         current_player->color != COLOR_BLACK)) {
        pthread_mutex_unlock(&match->lock);
        send_to_client(player_socket_fd, "ERROR|Not your turn\n");
        return 0;
    }

    /* 4️⃣ PARSE MOVE */
    Move move;
    notation_to_coords(from, &move.from_row, &move.from_col);
    notation_to_coords(to, &move.to_row, &move.to_col);

    move.piece = match->board.board[move.from_row][move.from_col];
    move.captured_piece = match->board.board[move.to_row][move.to_col];
    move.is_castling = 0;
    move.is_en_passant = 0;
    move.is_promotion = 0;
    move.promotion_piece = 0;

    /* 🔒 FAIL-SAFE #1: NEVER ALLOW KING CAPTURE */
    if (move.captured_piece == WHITE_KING ||
        move.captured_piece == BLACK_KING) {
        pthread_mutex_unlock(&match->lock);
        send_to_client(player_socket_fd,
                       "ERROR|Illegal move (cannot capture king)\n");
        return 0;
    }

    /* 5️⃣ VALIDATE MOVE */
    if (!validate_move(&match->board, &move, current_player->color)) {
        pthread_mutex_unlock(&match->lock);
        send_to_client(player_socket_fd, "ERROR|Invalid move\n");
        return 0;
    }

    /* 🔒 FAIL-SAFE #2: BOARD MUST STILL HAVE BOTH KINGS */
    if (!board_has_king(&match->board, COLOR_WHITE) ||
        !board_has_king(&match->board, COLOR_BLACK)) {
        pthread_mutex_unlock(&match->lock);
        send_to_client(player_socket_fd,
                       "ERROR|Invalid board state\n");
        return 0;
    }

    /* 6️⃣ EXECUTE MOVE */
    execute_move(&match->board, &move);

    /* 6.5️⃣ UPDATE TIMER */
    time_t now = time(NULL);
    int time_used = now - match->last_move_time;
    
    // Deduct time from current player
    if (current_player->color == COLOR_WHITE) {
        match->white_time_remaining -= time_used;
        match->white_time_remaining += 5;  // +5 seconds bonus
        if (match->white_time_remaining <= 0) {
            // White timeout
            force_end_game(match, db, match->black_player.user_id, "timeout");
            pthread_mutex_unlock(&match->lock);
            return 1;
        }
    } else {
        match->black_time_remaining -= time_used;
        match->black_time_remaining += 5;  // +5 seconds bonus
        if (match->black_time_remaining <= 0) {
            // Black timeout
            force_end_game(match, db, match->white_player.user_id, "timeout");
            pthread_mutex_unlock(&match->lock);
            return 1;
        }
    }
    
    match->last_move_time = now;

    /* 7️⃣ SEND TIMER UPDATE */
    char timer_msg[BUFFER_SIZE];
    snprintf(timer_msg, sizeof(timer_msg),
             "TIMER_UPDATE|%d|%d\n",
             match->white_time_remaining,
             match->black_time_remaining);
    broadcast_to_match(match, timer_msg, -1);

    /* 8️⃣ SAVE MOVE TO DB */
    char notation[16];
    snprintf(notation, sizeof(notation), "%s%s", from, to);
    history_save_move(db,
                      match->match_id,
                      current_player->user_id,
                      notation,
                      match->board.fen);

    /* 8️⃣ SEND RESPONSES */
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response),
             "MOVE_SUCCESS|%s|%s\n",
             notation,
             match->board.fen);
    send_to_client(player_socket_fd, response);

    if (opponent->socket_fd > 0 &&
        opponent->socket_fd != player_socket_fd) {
        snprintf(response, sizeof(response),
                 "OPPONENT_MOVE|%s|%s\n",
                 notation,
                 match->board.fen);
        send_to_client(opponent->socket_fd, response);
    }

    /* 9️⃣ CHECK GAME END */
    game_match_check_end_condition(match, db);

    pthread_mutex_unlock(&match->lock);
    return 1;
}

void force_white_win(GameMatch *match, PGconn *db)
{
    force_end_game(match,
                   db,
                   match->white_player.user_id,
                   "white_win");
}
void force_black_win(GameMatch *match, PGconn *db)
{
    force_end_game(match,
                   db,
                   match->black_player.user_id,
                   "black_win");
}


int game_match_check_end_condition(GameMatch *match, PGconn *db) {

    if (!board_has_king(&match->board, COLOR_WHITE)) {
        force_black_win(match, db);
        return 1;
    }
    if (!board_has_king(&match->board, COLOR_BLACK)) {
        force_white_win(match, db);
        return 1;
    }

    PlayerColor next_player = match->board.current_turn;
    
    printf("[DEBUG] game_match_check_end_condition: match_id=%d, current_turn=%s\n", 
           match->match_id, (next_player == COLOR_WHITE ? "WHITE" : "BLACK"));
    
    // Check checkmate
    int checkmate = is_checkmate(&match->board, next_player);
    printf("[DEBUG] is_checkmate() returned: %d\n", checkmate);
    
    if (checkmate) {
        match->status = GAME_FINISHED;
        match->result = (next_player == COLOR_WHITE) ? RESULT_BLACK_WIN : RESULT_WHITE_WIN;
        match->winner_id = (next_player == COLOR_WHITE) ? 
                          match->black_player.user_id : match->white_player.user_id;
        match->end_time = time(NULL);
        
        printf("[DEBUG] CHECKMATE DETECTED! Setting status=GAME_FINISHED, result=%s, winner_id=%d\n",
               (match->result == RESULT_WHITE_WIN ? "WHITE_WIN" : "BLACK_WIN"), match->winner_id);
        
        // Update database
        const char *result_str = (match->result == RESULT_WHITE_WIN) ? "white_win" : "black_win";
        history_update_match_result(db, match->match_id, result_str, match->winner_id);
        
        // Update ELO (only for PvP, not bot)
        if (match->black_player.user_id != 0) {
            int winner_id = match->winner_id;
            int loser_id = (winner_id == match->white_player.user_id) ? 
                           match->black_player.user_id : match->white_player.user_id;
            stats_update_elo(db, winner_id, loser_id);
        }
        
        // Stop timer
        timer_manager_stop_timer(&game_manager.timer_manager, match->match_id);
        
        // Broadcast game end
        char msg[BUFFER_SIZE];
        sprintf(msg, "GAME_END|checkmate|winner:%d\n", match->winner_id);
        broadcast_to_match(match, msg, -1);
        
        printf("[Match %d] CHECKMATE! Winner: %d\n", match->match_id, match->winner_id);
        
        // For bot match: send BOT_GAME_END if needed
        if (match->black_player.user_id == 0) {
            if (match->result == RESULT_WHITE_WIN) {
                sprintf(msg, "BOT_GAME_END|white_win|winner:%d\n", match->white_player.user_id);
            } else {
                sprintf(msg, "BOT_GAME_END|black_win|winner:bot\n");
            }
            send_to_client(match->white_player.socket_fd, msg);
        }
        
        return 1;
    }
    
    // Check stalemate
    if (!is_king_in_check(&match->board, next_player) &&
    is_stalemate(&match->board, next_player)){
        match->status = GAME_FINISHED;
        match->result = RESULT_DRAW;
        match->end_time = time(NULL);
        
        history_update_match_result(db, match->match_id, "draw", 0);
        
        // Stop timer
        timer_manager_stop_timer(&game_manager.timer_manager, match->match_id);
        
        char msg[BUFFER_SIZE];
        sprintf(msg, "GAME_END|stalemate|draw\n");
        broadcast_to_match(match, msg, -1);
        
        printf("[Match %d] STALEMATE! Draw\n", match->match_id);
        
        // For bot match: send BOT_GAME_END if needed
        if (match->black_player.user_id == 0) {
            sprintf(msg, "BOT_GAME_END|draw\n");
            send_to_client(match->white_player.socket_fd, msg);
        }
        
        return 1;
    }
    
    // Check 50-move rule
    if (match->board.halfmove_clock >= 100) {
        match->status = GAME_FINISHED;
        match->result = RESULT_DRAW;
        match->end_time = time(NULL);
        history_update_match_result(db, match->match_id, "draw", 0);
        timer_manager_stop_timer(&game_manager.timer_manager, match->match_id);
        char msg[BUFFER_SIZE];
        sprintf(msg, "GAME_END|fifty_move_rule|draw\n");
        broadcast_to_match(match, msg, -1);
        printf("[Match %d] Draw by 50-move rule\n", match->match_id);
        // For bot match: send BOT_GAME_END if needed
        if (match->black_player.user_id == 0) {
            sprintf(msg, "BOT_GAME_END|draw\n");
            send_to_client(match->white_player.socket_fd, msg);
        }
        return 1;
    }

    // Check insufficient material (chỉ còn mỗi vua hoặc không đủ quân để chiếu hết)
    int insufficient = is_insufficient_material(&match->board);
    printf("[DEBUG] is_insufficient_material() returned: %d\n", insufficient);
    
    if (insufficient) {
        match->status = GAME_FINISHED;
        match->result = RESULT_DRAW;
        match->end_time = time(NULL);
        
        printf("[DEBUG] INSUFFICIENT MATERIAL DETECTED! Updating database...\n");
        history_update_match_result(db, match->match_id, "draw", 0);
        
        timer_manager_stop_timer(&game_manager.timer_manager, match->match_id);
        char msg[BUFFER_SIZE];
        sprintf(msg, "GAME_END|insufficient_material|draw\n");
        broadcast_to_match(match, msg, -1);
        printf("[Match %d] Draw by insufficient material\n", match->match_id);
        // For bot match: send BOT_GAME_END if needed
        if (match->black_player.user_id == 0) {
            sprintf(msg, "BOT_GAME_END|draw\n");
            send_to_client(match->white_player.socket_fd, msg);
        }
        return 1;
    }
    
    // For bot match: check if black is bot and game ended
    if (match->black_player.user_id == 0 && match->status == GAME_FINISHED) {
        // Send bot game end response to player
        char msg[BUFFER_SIZE];
        if (match->result == RESULT_WHITE_WIN) {
            sprintf(msg, "BOT_GAME_END|white_win|winner:%d\n", match->white_player.user_id);
        } else if (match->result == RESULT_BLACK_WIN) {
            sprintf(msg, "BOT_GAME_END|black_win|winner:bot\n");
        } else {
            sprintf(msg, "BOT_GAME_END|draw\n");
        }
        send_to_client(match->white_player.socket_fd, msg);
        printf("[Bot Match %d] Game ended: %s\n", match->match_id, msg);
    }
    
    return 0;
}

void game_match_handle_surrender(GameMatch *match, int player_socket_fd, PGconn *db) {
    pthread_mutex_lock(&match->lock);
    
    if (match->white_player.socket_fd == player_socket_fd) {
        match->result = RESULT_BLACK_WIN;
        match->winner_id = match->black_player.user_id;
    } else {
        match->result = RESULT_WHITE_WIN;
        match->winner_id = match->white_player.user_id;
    }
    
    match->status = GAME_FINISHED;
    match->end_time = time(NULL);
    
    history_update_match_result(db, match->match_id, "surrender", match->winner_id);
    
    int winner_id = match->winner_id;
    int loser_id = (winner_id == match->white_player.user_id) ? 
                   match->black_player.user_id : match->white_player.user_id;
    stats_update_elo(db, winner_id, loser_id);
    
    // Stop timer
    timer_manager_stop_timer(&game_manager.timer_manager, match->match_id);
    
    char msg[BUFFER_SIZE];
    sprintf(msg, "GAME_END|surrender|winner:%d\n", match->winner_id);
    broadcast_to_match(match, msg, -1);
    
    // Send response to the surrendering player
    char response[BUFFER_SIZE];
    sprintf(response, "SURRENDER_SUCCESS|%d\n", match->winner_id);
    send_to_client(player_socket_fd, response);
    
    printf("[Match %d] Surrender! Winner: %d\n", match->match_id, match->winner_id);
    
    pthread_mutex_unlock(&match->lock);
}

void send_to_client(int socket_fd, const char *message) {
    send(socket_fd, message, strlen(message), 0);
}

void broadcast_to_match(GameMatch *match, const char *message, int exclude_fd) {
    if (match->white_player.socket_fd != exclude_fd && match->white_player.is_online) {
        send_to_client(match->white_player.socket_fd, message);
    }
    if (match->black_player.socket_fd != exclude_fd && match->black_player.is_online) {
        send_to_client(match->black_player.socket_fd, message);
    }
}

// Timer callback function for game timeout
void game_timeout_callback(int match_id) {
    printf("[Timer] Game %d timed out - ending game\n", match_id);
    
    // Find the match and end it with timeout
    GameMatch *match = game_manager_find_match(&game_manager, match_id);
    if (match) {
        pthread_mutex_lock(&match->lock);
        
        if (match->status == GAME_PLAYING) {
            match->status = GAME_FINISHED;
            match->result = RESULT_TIMEOUT;
            match->end_time = time(NULL);
            
            // Determine winner based on whose turn it was
            PlayerColor current_turn = match->board.current_turn;
            if (current_turn == COLOR_WHITE) {
                match->winner_id = match->black_player.user_id;
                match->result = RESULT_WHITE_TIMEOUT;
            } else {
                match->winner_id = match->white_player.user_id;
                match->result = RESULT_BLACK_TIMEOUT;
            }
            
            // Update database
            const char *result_str = (match->result == RESULT_WHITE_TIMEOUT) ? "white_timeout" : "black_timeout";
            history_update_match_result(db_conn, match->match_id, result_str, match->winner_id);
            
            // Update ELO
            int winner_id = match->winner_id;
            int loser_id = (winner_id == match->white_player.user_id) ? 
                           match->black_player.user_id : match->white_player.user_id;
            stats_update_elo(db_conn, winner_id, loser_id);
            
            // Broadcast timeout
            char msg[BUFFER_SIZE];
            sprintf(msg, "GAME_END|timeout|winner:%d\n", match->winner_id);
            broadcast_to_match(match, msg, -1);
            
            printf("[Match %d] TIMEOUT! Winner: %d\n", match->match_id, match->winner_id);
        }
        
        pthread_mutex_unlock(&match->lock);
    }
}

// ✅ NEW: Cleanup finished matches from memory
void game_manager_cleanup_finished_matches(GameManager *manager) {
    pthread_mutex_lock(&manager->lock);
    
    time_t now = time(NULL);
    int cleaned = 0;
    
    for (int i = 0; i < manager->match_count; ) {
        GameMatch *match = manager->matches[i];
        
        // Remove matches that finished more than 60 seconds ago
        if (match && match->status == GAME_FINISHED && 
            match->end_time > 0 && (now - match->end_time) > 60) {
            
            printf("[Cleanup] Removing finished match %d (ended %ld seconds ago)\n", 
                   match->match_id, now - match->end_time);
            
            // Stop timer
            timer_manager_stop_timer(&manager->timer_manager, match->match_id);
            
            // Free match
            pthread_mutex_destroy(&match->lock);
            free(match);
            
            // Shift remaining matches
            for (int j = i; j < manager->match_count - 1; j++) {
                manager->matches[j] = manager->matches[j + 1];
            }
            manager->matches[manager->match_count - 1] = NULL;
            manager->match_count--;
            cleaned++;
            
            // Don't increment i, check the same index again
        } else {
            i++;
        }
    }
    
    pthread_mutex_unlock(&manager->lock);
    
    if (cleaned > 0) {
        printf("[Cleanup] Cleaned %d finished matches. Active matches: %d\n", 
               cleaned, manager->match_count);
    }
}

// ✅ NEW: Force cleanup stale matches stuck in 'playing' state
void game_manager_force_cleanup_stale_matches(GameManager *manager) {
    pthread_mutex_lock(&manager->lock);
    
    time_t now = time(NULL);
    int cleaned = 0;
    
    for (int i = 0; i < manager->match_count; ) {
        GameMatch *match = manager->matches[i];
        
        // Remove matches that have been inactive for more than 30 minutes
        if (match && match->status == GAME_PLAYING && 
            (now - match->start_time) > 1800) {  // 30 minutes
            
            printf("[Force Cleanup] Removing stale match %d (started %ld seconds ago, no activity)\n", 
                   match->match_id, now - match->start_time);
            
            // Stop timer
            timer_manager_stop_timer(&manager->timer_manager, match->match_id);
            
            // Free match
            pthread_mutex_destroy(&match->lock);
            free(match);
            
            // Shift remaining matches
            for (int j = i; j < manager->match_count - 1; j++) {
                manager->matches[j] = manager->matches[j + 1];
            }
            manager->matches[manager->match_count - 1] = NULL;
            manager->match_count--;
            cleaned++;
            
        } else {
            i++;
        }
    }
    
    if (cleaned > 0) {
        printf("[Force Cleanup] Removed %d stale matches. Active matches: %d\n", 
               cleaned, manager->match_count);
    }
    
    pthread_mutex_unlock(&manager->lock);
}
