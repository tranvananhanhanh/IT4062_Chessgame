#include "game.h"
#include <ctype.h>

int execute_move(ChessBoard *board, Move *move) {
    // Handle en passant
    if (move->is_en_passant) {
        int captured_pawn_row = move->from_row;
        board->board[captured_pawn_row][move->to_col] = EMPTY;
    }
    
    // Handle castling
    if (move->is_castling) {
        int row = move->from_row;
        
        // Kingside
        if (move->to_col == 6) {
            board->board[row][7] = EMPTY;
            board->board[row][5] = is_white_piece(move->piece) ? WHITE_ROOK : BLACK_ROOK;
        }
        // Queenside
        else if (move->to_col == 2) {
            board->board[row][0] = EMPTY;
            board->board[row][3] = is_white_piece(move->piece) ? WHITE_ROOK : BLACK_ROOK;
        }
        
        // Update castling rights
        if (is_white_piece(move->piece)) {
            board->white_kingside_castle = 0;
            board->white_queenside_castle = 0;
        } else {
            board->black_kingside_castle = 0;
            board->black_queenside_castle = 0;
        }
    }
    
    // Handle promotion
    char piece_to_place = move->piece;
    if (move->is_promotion) {
        piece_to_place = move->promotion_piece;
    }
    
    // Move the piece
    board->board[move->to_row][move->to_col] = piece_to_place;
    board->board[move->from_row][move->from_col] = EMPTY;
    
    // Update king position
    if (toupper(move->piece) == 'K') {
        if (is_white_piece(move->piece)) {
            board->white_king_row = move->to_row;
            board->white_king_col = move->to_col;
        } else {
            board->black_king_row = move->to_row;
            board->black_king_col = move->to_col;
        }
    }
    
    // Update castling rights if rook moves
    if (toupper(move->piece) == 'R') {
        if (move->from_row == 7 && move->from_col == 0) {
            board->white_queenside_castle = 0;
        } else if (move->from_row == 7 && move->from_col == 7) {
            board->white_kingside_castle = 0;
        } else if (move->from_row == 0 && move->from_col == 0) {
            board->black_queenside_castle = 0;
        } else if (move->from_row == 0 && move->from_col == 7) {
            board->black_kingside_castle = 0;
        }
    }
    
    // Update castling rights if rook is captured
    if (move->captured_piece != EMPTY && toupper(move->captured_piece) == 'R') {
        if (move->to_row == 7 && move->to_col == 0) {
            board->white_queenside_castle = 0;
        } else if (move->to_row == 7 && move->to_col == 7) {
            board->white_kingside_castle = 0;
        } else if (move->to_row == 0 && move->to_col == 0) {
            board->black_queenside_castle = 0;
        } else if (move->to_row == 0 && move->to_col == 7) {
            board->black_kingside_castle = 0;
        }
    }
    
    // Reset en passant
    board->en_passant_file = -1;
    board->en_passant_rank = -1;
    
    // Update halfmove clock (for 50-move rule)
    if (toupper(move->piece) == 'P' || move->captured_piece != EMPTY) {
        board->halfmove_clock = 0;
    } else {
        board->halfmove_clock++;
    }
    
    // Switch turn
    board->current_turn = (board->current_turn == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;
    board->move_count++;
    
    // Update FEN
    chess_board_to_fen(board, board->fen);
    
    return 1;
}