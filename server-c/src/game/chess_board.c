#include "game.h"
#include <ctype.h>

void chess_board_init(ChessBoard *board) {
    // Rank 8 (Black)
    board->board[0][0] = BLACK_ROOK;
    board->board[0][1] = BLACK_KNIGHT;
    board->board[0][2] = BLACK_BISHOP;
    board->board[0][3] = BLACK_QUEEN;
    board->board[0][4] = BLACK_KING;
    board->board[0][5] = BLACK_BISHOP;
    board->board[0][6] = BLACK_KNIGHT;
    board->board[0][7] = BLACK_ROOK;
    
    // Rank 7 (Black pawns)
    for (int i = 0; i < 8; i++) board->board[1][i] = BLACK_PAWN;
    
    // Ranks 6-3 (Empty)
    for (int row = 2; row < 6; row++)
        for (int col = 0; col < 8; col++)
            board->board[row][col] = EMPTY;
    
    // Rank 2 (White pawns)
    for (int i = 0; i < 8; i++) board->board[6][i] = WHITE_PAWN;
    
    // Rank 1 (White)
    board->board[7][0] = WHITE_ROOK;
    board->board[7][1] = WHITE_KNIGHT;
    board->board[7][2] = WHITE_BISHOP;
    board->board[7][3] = WHITE_QUEEN;
    board->board[7][4] = WHITE_KING;
    board->board[7][5] = WHITE_BISHOP;
    board->board[7][6] = WHITE_KNIGHT;
    board->board[7][7] = WHITE_ROOK;
    
    board->current_turn = COLOR_WHITE;
    board->move_count = 0;
    board->halfmove_clock = 0;
    
    board->white_kingside_castle = 1;
    board->white_queenside_castle = 1;
    board->black_kingside_castle = 1;
    board->black_queenside_castle = 1;
    
    board->en_passant_file = -1;
    board->en_passant_rank = -1;
    
    board->white_king_row = 7;
    board->white_king_col = 4;
    board->black_king_row = 0;
    board->black_king_col = 4;
    
    chess_board_to_fen(board, board->fen);
}

void chess_board_copy(ChessBoard *dest, ChessBoard *src) {
    memcpy(dest, src, sizeof(ChessBoard));
}

void chess_board_to_fen(ChessBoard *board, char *fen) {
    char temp[256] = "";
    
    // Board position
    for (int row = 0; row < 8; row++) {
        int empty_count = 0;
        for (int col = 0; col < 8; col++) {
            char piece = board->board[row][col];
            if (piece == EMPTY) {
                empty_count++;
            } else {
                if (empty_count > 0) {
                    char num[12];
                    sprintf(num, "%d", empty_count);
                    strcat(temp, num);
                    empty_count = 0;
                }
                char piece_str[2] = {piece, '\0'};
                strcat(temp, piece_str);
            }
        }
        if (empty_count > 0) {
            char num[12];
            sprintf(num, "%d", empty_count);
            strcat(temp, num);
        }
        if (row < 7) strcat(temp, "/");
    }
    
    strcat(temp, board->current_turn == COLOR_WHITE ? " w " : " b ");
    
    char castling[5] = "";
    if (board->white_kingside_castle) strcat(castling, "K");
    if (board->white_queenside_castle) strcat(castling, "Q");
    if (board->black_kingside_castle) strcat(castling, "k");
    if (board->black_queenside_castle) strcat(castling, "q");
    if (strlen(castling) == 0) strcat(castling, "-");
    strcat(temp, castling);
    strcat(temp, " ");
    
    if (board->en_passant_file >= 0) {
        char ep[3];
        sprintf(ep, "%c%d", 'a' + board->en_passant_file, board->en_passant_rank);
        strcat(temp, ep);
    } else {
        strcat(temp, "-");
    }
    
    char moves[32];
    sprintf(moves, " %d %d", board->halfmove_clock, (board->move_count / 2) + 1);
    strcat(temp, moves);
    
    strcpy(fen, temp);
}

void chess_board_print(ChessBoard *board) {
    printf("\n  +---+---+---+---+---+---+---+---+\n");
    for (int row = 0; row < 8; row++) {
        printf("%d |", 8 - row);
        for (int col = 0; col < 8; col++) {
            printf(" %c |", board->board[row][col]);
        }
        printf("\n  +---+---+---+---+---+---+---+---+\n");
    }
    printf("    a   b   c   d   e   f   g   h\n");
}

void notation_to_coords(const char *notation, int *row, int *col) {
    *col = notation[0] - 'a';
    *row = 8 - (notation[1] - '0');
}

void coords_to_notation(int row, int col, char *notation) {
    notation[0] = 'a' + col;
    notation[1] = '0' + (8 - row);
    notation[2] = '\0';
}

char get_piece_at(ChessBoard *board, int row, int col) {
    if (row < 0 || row >= 8 || col < 0 || col >= 8) return '\0';
    return board->board[row][col];
}

void set_piece_at(ChessBoard *board, int row, int col, char piece) {
    if (row < 0 || row >= 8 || col < 0 || col >= 8) return;
    board->board[row][col] = piece;
    
    if (piece == WHITE_KING) {
        board->white_king_row = row;
        board->white_king_col = col;
    } else if (piece == BLACK_KING) {
        board->black_king_row = row;
        board->black_king_col = col;
    }
}

int is_white_piece(char piece) {
    return piece >= 'A' && piece <= 'Z';
}

int is_black_piece(char piece) {
    return piece >= 'a' && piece <= 'z';
}

PlayerColor get_piece_color(char piece) {
    return is_white_piece(piece) ? COLOR_WHITE : COLOR_BLACK;
}
