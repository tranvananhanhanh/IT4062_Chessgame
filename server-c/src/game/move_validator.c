#include "game.h"
#include <stdlib.h>
#include <ctype.h>

int validate_move(ChessBoard *board, Move *move, PlayerColor player_color) {
    // Check bounds
    if (move->from_row < 0 || move->from_row >= 8 || move->from_col < 0 || move->from_col >= 8) return 0;
    if (move->to_row < 0 || move->to_row >= 8 || move->to_col < 0 || move->to_col >= 8) return 0;
    
    // Check it's player's turn
    if (board->current_turn != player_color) return 0;
    
    // Check source square has a piece
    char piece = board->board[move->from_row][move->from_col];
    if (piece == EMPTY) return 0;
    
    // Check piece belongs to player
    if (get_piece_color(piece) != player_color) return 0;
    
    // Check destination is not own piece
    char dest_piece = board->board[move->to_row][move->to_col];
    if (dest_piece != EMPTY && get_piece_color(dest_piece) == player_color) return 0;
    
    move->piece = piece;
    move->captured_piece = dest_piece;
    // ❌ King can never be captured
    if (dest_piece == WHITE_KING || dest_piece == BLACK_KING) {
        return 0;
    }

    
    // Validate based on piece type
    int valid = 0;
    char piece_type = toupper(piece);
    
    switch (piece_type) {
        case 'P': valid = validate_pawn_move(board, move); break;
        case 'N': valid = validate_knight_move(board, move); break;
        case 'B': valid = validate_bishop_move(board, move); break;
        case 'R': valid = validate_rook_move(board, move); break;
        case 'Q': valid = validate_queen_move(board, move); break;
        case 'K': valid = validate_king_move(board, move); break;
        default: return 0;
    }
    
    if (!valid) return 0;
    
    // Test if move leaves king in check
    ChessBoard test_board;
    chess_board_copy(&test_board, board);
    
    Move test_move = *move;
    execute_move(&test_board, &test_move);
    
    if (is_king_in_check(&test_board, player_color)) {
        return 0;
    }
    
    return 1;
}

int validate_pawn_move(ChessBoard *board, Move *move) {
    int direction = is_white_piece(move->piece) ? -1 : 1;
    int start_row = is_white_piece(move->piece) ? 6 : 1;

    int row_diff = move->to_row - move->from_row;
    int col_diff = abs(move->to_col - move->from_col);

    /* =====================
       Forward one square
       ===================== */
    if (col_diff == 0 && row_diff == direction) {
        if (board->board[move->to_row][move->to_col] == EMPTY) {

            /* Promotion */
            if (move->to_row == 0 || move->to_row == 7) {
                move->is_promotion = 1;

                /* Default to Queen ONLY if not specified */
                if (move->promotion_piece == '\0') {
                    move->promotion_piece = is_white_piece(move->piece)
                        ? WHITE_QUEEN
                        : BLACK_QUEEN;
                }
            }
            return 1;
        }
    }

    /* =====================
       Forward two squares
       ===================== */
    if (col_diff == 0 &&
        row_diff == 2 * direction &&
        move->from_row == start_row) {

        int middle_row = move->from_row + direction;

        if (board->board[middle_row][move->from_col] == EMPTY &&
            board->board[move->to_row][move->to_col] == EMPTY) {

            board->en_passant_file = move->from_col;
            board->en_passant_rank = middle_row;
            return 1;
        }
    }

    /* =====================
       Capture diagonally
       ===================== */
    if (col_diff == 1 && row_diff == direction) {

        /* Normal capture */
        if (board->board[move->to_row][move->to_col] != EMPTY) {

            /* Promotion */
            if (move->to_row == 0 || move->to_row == 7) {
                move->is_promotion = 1;

                if (move->promotion_piece == '\0') {
                    move->promotion_piece = is_white_piece(move->piece)
                        ? WHITE_QUEEN
                        : BLACK_QUEEN;
                }
            }
            return 1;
        }

        /* En passant */
        if (move->to_col == board->en_passant_file &&
            move->to_row == board->en_passant_rank) {

            move->is_en_passant = 1;
            return 1;
        }
    }

    return 0;
}


int validate_knight_move(ChessBoard *board, Move *move) {
    (void)board;  // Suppress unused parameter warning
    int row_diff = abs(move->to_row - move->from_row);
    int col_diff = abs(move->to_col - move->from_col);
    
    return (row_diff == 2 && col_diff == 1) || (row_diff == 1 && col_diff == 2);
}

int validate_bishop_move(ChessBoard *board, Move *move) {
    int row_diff = abs(move->to_row - move->from_row);
    int col_diff = abs(move->to_col - move->from_col);
    
    if (row_diff != col_diff) return 0;
    
    return is_path_clear(board, move->from_row, move->from_col, move->to_row, move->to_col);
}

int validate_rook_move(ChessBoard *board, Move *move) {
    if (move->from_row != move->to_row && move->from_col != move->to_col) return 0;
    
    return is_path_clear(board, move->from_row, move->from_col, move->to_row, move->to_col);
}

int validate_queen_move(ChessBoard *board, Move *move) {
    return validate_bishop_move(board, move) || validate_rook_move(board, move);
}

int validate_king_move(ChessBoard *board, Move *move) {
    int row_diff = abs(move->to_row - move->from_row);
    int col_diff = abs(move->to_col - move->from_col);
    
    // Normal king move
    if (row_diff <= 1 && col_diff <= 1) {
        return 1;
    }
    
    // Castling
    if (row_diff == 0 && col_diff == 2) {
        PlayerColor color = get_piece_color(move->piece);
        int row = move->from_row;
        
        // Kingside castling
        if (move->to_col == 6) {
            if (color == COLOR_WHITE && !board->white_kingside_castle) return 0;
            if (color == COLOR_BLACK && !board->black_kingside_castle) return 0;
            
            if (board->board[row][5] != EMPTY || board->board[row][6] != EMPTY) return 0;
            if (board->board[row][7] != (color == COLOR_WHITE ? WHITE_ROOK : BLACK_ROOK)) return 0;
            
            if (is_square_attacked(board, row, 4, color == COLOR_WHITE ? COLOR_BLACK : COLOR_WHITE)) return 0;
            if (is_square_attacked(board, row, 5, color == COLOR_WHITE ? COLOR_BLACK : COLOR_WHITE)) return 0;
            if (is_square_attacked(board, row, 6, color == COLOR_WHITE ? COLOR_BLACK : COLOR_WHITE)) return 0;
            
            move->is_castling = 1;
            return 1;
        }
        
        // Queenside castling
        if (move->to_col == 2) {
            if (color == COLOR_WHITE && !board->white_queenside_castle) return 0;
            if (color == COLOR_BLACK && !board->black_queenside_castle) return 0;
            
            if (board->board[row][1] != EMPTY || board->board[row][2] != EMPTY || 
                board->board[row][3] != EMPTY) return 0;
            if (board->board[row][0] != (color == COLOR_WHITE ? WHITE_ROOK : BLACK_ROOK)) return 0;
            
            if (is_square_attacked(board, row, 4, color == COLOR_WHITE ? COLOR_BLACK : COLOR_WHITE)) return 0;
            if (is_square_attacked(board, row, 3, color == COLOR_WHITE ? COLOR_BLACK : COLOR_WHITE)) return 0;
            if (is_square_attacked(board, row, 2, color == COLOR_WHITE ? COLOR_BLACK : COLOR_WHITE)) return 0;
            
            move->is_castling = 1;
            return 1;
        }
    }
    
    return 0;
}

int is_path_clear(ChessBoard *board, int from_row, int from_col, int to_row, int to_col) {
    int row_step = (to_row > from_row) ? 1 : ((to_row < from_row) ? -1 : 0);
    int col_step = (to_col > from_col) ? 1 : ((to_col < from_col) ? -1 : 0);
    
    int row = from_row + row_step;
    int col = from_col + col_step;
    
    while (row != to_row || col != to_col) {
        if (board->board[row][col] != EMPTY) return 0;
        row += row_step;
        col += col_step;
    }
    
    return 1;
}

int is_square_attacked(ChessBoard *board, int row, int col, PlayerColor attacker_color) {
    // Check all opponent pieces to see if they can attack this square
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            char piece = board->board[r][c];
            if (piece == EMPTY) continue;
            if (get_piece_color(piece) != attacker_color) continue;
            
            Move test_move = {r, c, row, col, piece, board->board[row][col], 0, 0, 0, 0};
            
            char piece_type = toupper(piece);
            int can_attack = 0;
            
            switch (piece_type) {
                case 'P': {
                    int direction = is_white_piece(piece) ? -1 : 1;
                    int row_diff = row - r;
                    int col_diff = abs(col - c);
                    can_attack = (col_diff == 1 && row_diff == direction);
                    break;
                }
                case 'N': can_attack = validate_knight_move(board, &test_move); break;
                case 'B': can_attack = validate_bishop_move(board, &test_move); break;
                case 'R': can_attack = validate_rook_move(board, &test_move); break;
                case 'Q': can_attack = validate_queen_move(board, &test_move); break;
                case 'K': {
                    int rd = abs(row - r);
                    int cd = abs(col - c);
                    can_attack = (rd <= 1 && cd <= 1);
                    break;
                }
            }
            
            if (can_attack) return 1;
        }
    }
    
    return 0;
}

int is_king_in_check(ChessBoard *board, PlayerColor king_color) {
    int king_row, king_col;
    
    if (king_color == COLOR_WHITE) {
        king_row = board->white_king_row;
        king_col = board->white_king_col;
    } else {
        king_row = board->black_king_row;
        king_col = board->black_king_col;
    }
    
    PlayerColor attacker_color = (king_color == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;
    return is_square_attacked(board, king_row, king_col, attacker_color);
}
