import React, { useState } from 'react';

const CHESS_PIECES = {
    p: '♟', n: '♞', b: '♝', r: '♜', q: '♛', k: '♚',
    P: '♙', N: '♘', B: '♗', R: '♖', Q: '♕', K: '♔',
};

function parseFen(fenString) {
    const [position] = fenString.split(' ');
    const rows = position.split('/');
    const board = [];
    for (const row of rows) {
        const rowArray = [];
        for (const ch of row) {
            if (/\d/.test(ch)) {
                for (let i = 0; i < parseInt(ch, 10); i++) rowArray.push(null);
            } else {
                rowArray.push(ch);
            }
        }
        board.push(rowArray);
    }
    return board;
}

const ChessBoard = ({ fen, onSquareClick }) => {
    const [selectedSquare, setSelectedSquare] = useState(null);
    const board = parseFen(fen);
    const squares = [];

    const toSquareName = (row, col) => {
        const file = String.fromCharCode('a'.charCodeAt(0) + col);
        const rank = 8 - row;
        return `${file}${rank}`;
    };

    const handleSquareClick = (row, col) => {
        const squareName = toSquareName(row, col);
        if (!selectedSquare) {
            if (board[row][col]) setSelectedSquare(squareName);
        } else {
            const move = `${selectedSquare}${squareName}`;
            onSquareClick(move);
            setSelectedSquare(null);
        }
    };

    for (let r = 0; r < 8; r++) {
        for (let c = 0; c < 8; c++) {
            const squareName = toSquareName(r, c);
            const piece = board[r][c];
            const isLight = (r + c) % 2 === 0;
            const bg = isLight ? '#e5e7eb' : '#4b5563'; // fallback if Tailwind not active
            const isWhitePiece = piece && piece === piece.toUpperCase();
            const pieceColorCss = isWhitePiece ? 'text-white' : 'text-gray-900';
            const pieceColor = isWhitePiece ? '#ffffff' : '#111827';
            const isSelected = selectedSquare === squareName ? 'ring-4 ring-yellow-500 ring-offset-1' : '';
            squares.push(
                <div
                    key={squareName}
                    className={`w-full h-full flex items-center justify-center text-4xl cursor-pointer transition-all duration-150 rounded ${isLight ? 'bg-gray-200' : 'bg-gray-600'} ${isSelected}`}
                    onClick={() => handleSquareClick(r, c)}
                    style={{ aspectRatio: '1 / 1', backgroundColor: bg }}
                    data-testid={`square-${squareName}`}
                    role="button"
                    aria-label={`square ${squareName}`}
                >
                    <span className={pieceColorCss} style={{ color: pieceColor, fontSize: '2rem', lineHeight: 1 }}>
                        {piece ? CHESS_PIECES[piece] : ''}
                    </span>
                </div>
            );
        }
    }

    return (
        <div
            className="w-full max-w-lg shadow-2xl rounded-2xl overflow-hidden bg-gray-400"
            style={{ display: 'grid', gridTemplateColumns: 'repeat(8, 1fr)', gridTemplateRows: 'repeat(8, 1fr)', backgroundColor: '#9ca3af' }}
        >
            {squares}
        </div>
    );
};

export default ChessBoard;
