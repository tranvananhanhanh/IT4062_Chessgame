import React from 'react';
import './ChessBoard.css';

const PIECE_SYMBOLS = {
    w: {
        k: '♔', q: '♕', r: '♖', b: '♗', n: '♘', p: '♙',
    },
    b: {
        k: '♚', q: '♛', r: '♜', b: '♝', n: '♞', p: '♟︎',
    },
};
const FILES = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'];

const getSquareColor = (row, col) => ((row + col) % 2 === 1 ? 'dark' : 'light');

function ChessBoard({
    board, // chess.js instance or FEN string
    selectedSquare,
    validMoves = [],
    lastMove = null,
    onSquareClick,
}) {
    // Get piece at square
    const getPiece = (square) => {
        if (!board) return null;
        if (typeof board.get === 'function') return board.get(square);
        // If board is FEN string, parse manually (not implemented here)
        return null;
    };

    return (
        <div className="board-shell">
            <div className="board">
                {Array.from({ length: 8 }).map((_, rowIdx) => (
                    <div className="board-row" key={`row-${rowIdx}`}>
                        {Array.from({ length: 8 }).map((_, colIdx) => {
                            const square = FILES[colIdx] + (8 - rowIdx);
                            const piece = getPiece(square);
                            const isSelected = selectedSquare === square;
                            const isValidMove = validMoves.includes(square);
                            const isLastMoveSquare = lastMove && (lastMove.from === square || lastMove.to === square);
                            return (
                                <div
                                    key={square}
                                    className={`board-square ${getSquareColor(rowIdx, colIdx)}${isSelected ? ' selected' : ''}${isValidMove ? ' valid-move' : ''}${isLastMoveSquare ? ' last-move' : ''}`}
                                    onClick={() => onSquareClick && onSquareClick(rowIdx, colIdx, square)}
                                >
                                    {piece ? <span className={`piece ${piece.color}`}>{PIECE_SYMBOLS[piece.color][piece.type]}</span> : null}
                                </div>
                            );
                        })}
                    </div>
                ))}
            </div>
            <div className="rank-labels rank-left">
                {Array.from({ length: 8 }, (_, idx) => 8 - idx).map((rank) => (
                    <span key={`left-${rank}`}>{rank}</span>
                ))}
            </div>
            <div className="rank-labels rank-right">
                {Array.from({ length: 8 }, (_, idx) => 8 - idx).map((rank) => (
                    <span key={`right-${rank}`}>{rank}</span>
                ))}
            </div>
            <div className="file-labels file-bottom">
                {FILES.map((file) => (
                    <span key={`bottom-${file}`}>{file.toUpperCase()}</span>
                ))}
            </div>
            <div className="file-labels file-top">
                {FILES.map((file) => (
                    <span key={`top-${file}`}>{file.toUpperCase()}</span>
                ))}
            </div>
        </div>
    );
}

export default ChessBoard;
