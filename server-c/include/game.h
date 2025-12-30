#ifndef GAME_H
#define GAME_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <libpq-fe.h>
#include "timer.h"

#define BOARD_SIZE 8
#define FEN_MAX_LENGTH 256
#define MAX_MATCHES 100
#define BUFFER_SIZE 4096

// Piece types
typedef enum {
    EMPTY = '.',
    WHITE_PAWN = 'P',
    WHITE_KNIGHT = 'N',
    WHITE_BISHOP = 'B',
    WHITE_ROOK = 'R',
    WHITE_QUEEN = 'Q',
    WHITE_KING = 'K',
    BLACK_PAWN = 'p',
    BLACK_KNIGHT = 'n',
    BLACK_BISHOP = 'b',
    BLACK_ROOK = 'r',
    BLACK_QUEEN = 'q',
    BLACK_KING = 'k'
} PieceType;

// Game status
typedef enum {
    GAME_WAITING,
    GAME_PLAYING,
    GAME_PAUSED = 2,      // ✅ NEW
    GAME_FINISHED,
    GAME_ABORTED,
} GameStatus;

// Game result
typedef enum {
    RESULT_NONE,
    RESULT_WHITE_WIN,
    RESULT_BLACK_WIN,
    RESULT_DRAW,
    RESULT_SURRENDER,
    RESULT_TIMEOUT,
    RESULT_WHITE_TIMEOUT,
    RESULT_BLACK_TIMEOUT
} GameResult;

// Player color
typedef enum {
    COLOR_WHITE,
    COLOR_BLACK
} PlayerColor;

// Move structure
typedef struct {
    int from_row;
    int from_col;
    int to_row;
    int to_col;
    char piece;
    char captured_piece;
    int is_castling;
    int is_en_passant;
    int is_promotion;
    char promotion_piece;
} Move;

// Chess board structure
typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];
    char fen[FEN_MAX_LENGTH];
    PlayerColor current_turn;
    int move_count;
    int halfmove_clock;
    
    // Castling rights
    int white_kingside_castle;
    int white_queenside_castle;
    int black_kingside_castle;
    int black_queenside_castle;
    
    // En passant
    int en_passant_file;
    int en_passant_rank;
    
    // King positions
    int white_king_row;
    int white_king_col;
    int black_king_row;
    int black_king_col;
} ChessBoard;

// Player info
typedef struct {
    int user_id;
    int socket_fd;
    PlayerColor color;
    int is_online;
    char username[64];
} Player;

// Game match structure
typedef struct GameMatch {
    int match_id;
    GameStatus status;
    Player white_player;
    Player black_player;
    ChessBoard board;
    time_t start_time;
    time_t end_time;
    GameResult result;
    int winner_id;
    int draw_requester_id;  // ID of player who requested draw (0 if none)
    int rematch_id;         // ID of new match if rematch accepted (0 if none)
    int rematch_requester_id;   // ID người xin rematch

    pthread_mutex_t lock;
    char bot_difficulty[16]; // Difficulty for bot games ("easy", "hard", etc.)
} GameMatch;

// Game manager
typedef struct {
    GameMatch *matches[MAX_MATCHES];
    int match_count;
    TimerManager timer_manager;
    pthread_mutex_t lock;
} GameManager;

// ============ CHESS BOARD FUNCTIONS ============
void chess_board_init(ChessBoard *board);
void chess_board_copy(ChessBoard *dest, ChessBoard *src);
void chess_board_to_fen(ChessBoard *board, char *fen);
void chess_board_print(ChessBoard *board);

// ============ MOVE VALIDATION ============
int validate_move(ChessBoard *board, Move *move, PlayerColor player_color);
int is_square_attacked(ChessBoard *board, int row, int col, PlayerColor attacker_color);
int is_king_in_check(ChessBoard *board, PlayerColor king_color);
int validate_pawn_move(ChessBoard *board, Move *move);
int validate_knight_move(ChessBoard *board, Move *move);
int validate_bishop_move(ChessBoard *board, Move *move);
int validate_rook_move(ChessBoard *board, Move *move);
int validate_queen_move(ChessBoard *board, Move *move);
int validate_king_move(ChessBoard *board, Move *move);

// ============ MOVE EXECUTION ============
int execute_move(ChessBoard *board, Move *move);
int is_path_clear(ChessBoard *board, int from_row, int from_col, int to_row, int to_col);

// ============ GAME STATE DETECTION ============
int is_checkmate(ChessBoard *board, PlayerColor player_color);
int is_stalemate(ChessBoard *board, PlayerColor player_color);
int has_legal_moves(ChessBoard *board, PlayerColor player_color);
int is_insufficient_material(const ChessBoard *board);

// ============ UTILITY FUNCTIONS ============
void notation_to_coords(const char *notation, int *row, int *col);
void coords_to_notation(int row, int col, char *notation);
char get_piece_at(ChessBoard *board, int row, int col);
void set_piece_at(ChessBoard *board, int row, int col, char piece);
int is_white_piece(char piece);
int is_black_piece(char piece);
PlayerColor get_piece_color(char piece);

// ============ GAME MANAGER ============
void game_manager_init(GameManager *manager);
GameMatch* game_manager_create_match(GameManager *manager, Player white, Player black, PGconn *db);
GameMatch* game_manager_create_bot_match(GameManager *manager, Player white, Player black, PGconn *db, const char *difficulty);
GameMatch* game_manager_find_match(GameManager *manager, int match_id);
GameMatch* game_manager_find_match_by_player(GameManager *manager, int socket_fd);
void game_manager_remove_match(GameManager *manager, int match_id);
void game_manager_cleanup_finished_matches(GameManager *manager);
void game_manager_force_cleanup_stale_matches(GameManager *manager);

// ============ GAME MATCH ============
int game_match_make_move(GameMatch *match, int player_socket_fd, int player_id, const char *from, const char *to, PGconn *db);
int game_match_check_end_condition(GameMatch *match, PGconn *db);
void game_match_handle_surrender(GameMatch *match, int player_socket_fd, PGconn *db);
void game_timeout_callback(int match_id);

// ============ NETWORK ============
void send_to_client(int socket_fd, const char *message);
void broadcast_to_match(GameMatch *match, const char *message, int exclude_fd);

#endif
