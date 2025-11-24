#include "game.h"

int has_legal_moves(ChessBoard *board, PlayerColor player_color) {
    // Try all possible moves for all pieces of this color
    for (int from_row = 0; from_row < 8; from_row++) {
        for (int from_col = 0; from_col < 8; from_col++) {
            char piece = board->board[from_row][from_col];
            if (piece == EMPTY) continue;
            if (get_piece_color(piece) != player_color) continue;
            
            // Try all possible destination squares
            for (int to_row = 0; to_row < 8; to_row++) {
                for (int to_col = 0; to_col < 8; to_col++) {
                    if (from_row == to_row && from_col == to_col) continue;
                    
                    Move move = {from_row, from_col, to_row, to_col, piece, 
                                board->board[to_row][to_col], 0, 0, 0, 0};
                    
                    if (validate_move(board, &move, player_color)) {
                        return 1; // Found at least one legal move
                    }
                }
            }
        }
    }
    
    return 0; // No legal moves found
}

int is_checkmate(ChessBoard *board, PlayerColor player_color) {
    // Checkmate = in check AND no legal moves
    if (!is_king_in_check(board, player_color)) return 0;
    return !has_legal_moves(board, player_color);
}

int is_stalemate(ChessBoard *board, PlayerColor player_color) {
    // Stalemate = NOT in check AND no legal moves
    if (is_king_in_check(board, player_color)) return 0;
    return !has_legal_moves(board, player_color);
}