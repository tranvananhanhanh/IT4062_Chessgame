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

int is_insufficient_material(const ChessBoard *board) {
    // Count pieces on the board
    int white_minor = 0, black_minor = 0;
    int white_other = 0, black_other = 0;
    
    printf("[DEBUG is_insufficient_material] Checking board...\n");
    
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            char p = board->board[r][c];
            if (p == EMPTY) continue;
            if (p == WHITE_KING || p == BLACK_KING) continue;
            
            printf("[DEBUG] Found piece '%c' at (%d,%d)\n", p, r, c);
            
            // Count minor pieces (bishop, knight)
            if (p == WHITE_BISHOP || p == WHITE_KNIGHT) {
                white_minor++;
            } else if (p == BLACK_BISHOP || p == BLACK_KNIGHT) {
                black_minor++;
            }
            // Count other pieces (pawn, rook, queen)
            else if (p == WHITE_PAWN || p == WHITE_ROOK || p == WHITE_QUEEN) {
                white_other++;
            } else if (p == BLACK_PAWN || p == BLACK_ROOK || p == BLACK_QUEEN) {
                black_other++;
            }
        }
    }
    
    printf("[DEBUG is_insufficient_material] white_minor=%d, black_minor=%d, white_other=%d, black_other=%d\n",
           white_minor, black_minor, white_other, black_other);
    
    // Case 1: Chỉ còn mỗi vua (King vs King)
    if (white_minor == 0 && black_minor == 0 && white_other == 0 && black_other == 0) {
        return 1;
    }
    
    // Case 2: Vua vs vua + tượng hoặc mã (King vs King+Bishop/Knight)
    if ((white_minor == 1 && white_other == 0 && black_minor == 0 && black_other == 0) ||
        (black_minor == 1 && black_other == 0 && white_minor == 0 && white_other == 0)) {
        return 1;
    }
    
    // Case 3: Vua + tượng vs vua + tượng (cùng màu ô) - simplified check
    // (Nâng cao hơn cần kiểm tra xem 2 tượng có cùng màu ô hay không)
    
    return 0;
}
