import { useState, useEffect, useMemo } from 'react';
import { Chess } from 'chess.js';
import api from '../../api/api';
import './ChessGameBot.css';
import ChessBoard from '../chess/ChessBoard';

const getValidMoves = (game, selected) => {
    if (!selected) return [];
    const moves = game.moves({ square: selected, verbose: true });
    return moves.map((m) => m.to);
};

const FILES = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'];

const ChessGameBot = () => {
    const [game, setGame] = useState(new Chess());
    const [gameState, setGameState] = useState({
        matchId: null,
        playerId: 1,
        playerName: 'You',
        botName: 'Bot',
        playerColor: 'white',
        isMyTurn: true,
        gameStatus: 'waiting',
        winner: null,
    });
    const [moveHistory, setMoveHistory] = useState([]);
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState('');
    const [selectedSquare, setSelectedSquare] = useState(null);
    const [lastMove, setLastMove] = useState(null);

    // Khởi tạo trận đấu với bot
    const startBotGame = async () => {
        setLoading(true);
        setError('');
        try {
            const result = await api.post('/mode/bot', { user_id: gameState.playerId });
            if (result.data.match_id) {
                setGameState(prev => ({
                    ...prev,
                    matchId: result.data.match_id,
                    gameStatus: 'playing',
                    isMyTurn: true,
                    winner: null,
                }));
                setGame(new Chess());
                setMoveHistory([]);
                setSelectedSquare(null);
                setLastMove(null);
            } else {
                setError('Không thể khởi tạo trận đấu với bot!');
            }
        } catch (err) {
            setError('Lỗi khởi tạo trận đấu: ' + err.message);
        } finally {
            setLoading(false);
        }
    };

    // Xử lý nước đi của người chơi
    const handleSquareClick = async (row, col) => {
        // ✅ Block moves if game is not playing (paused, finished, waiting)
        if (!gameState.isMyTurn || gameState.gameStatus !== 'playing') {
            if (gameState.gameStatus === 'paused') {
                setError('Game đang tạm dừng. Không thể di chuyển!');
            }
            return;
        }
        const square = FILES[col] + (8 - row);
        const piece = game.get(square);
        if (selectedSquare) {
            // Nếu click vào chính quân đã chọn, bỏ chọn
            if (selectedSquare === square) {
                setSelectedSquare(null);
                return;
            }
            // Nếu click vào ô hợp lệ, thực hiện nước đi
            const validMoves = getValidMoves(game, selectedSquare);
            if (validMoves.includes(square)) {
                setLoading(true);
                try {
                    // ✅ Check if this is a pawn promotion
                    const selectedPiece = game.get(selectedSquare);
                    const isPromotion = selectedPiece &&
                        selectedPiece.type === 'p' &&
                        ((selectedPiece.color === 'w' && square[1] === '8') ||
                            (selectedPiece.color === 'b' && square[1] === '1'));

                    const move = game.move({
                        from: selectedSquare,
                        to: square,
                        promotion: isPromotion ? 'q' : undefined
                    });

                    if (!move) {
                        setError('Nước đi không hợp lệ');
                        setLoading(false);
                        return;
                    }

                    // ✅ Send move with promotion piece if applicable
                    const moveNotation = isPromotion ?
                        `${selectedSquare}${square}q` :
                        `${selectedSquare}${square}`;

                    // Gửi nước đi lên server
                    const result = await api.post('/game/bot/move', {
                        match_id: gameState.matchId,
                        move: moveNotation,
                    });
                    if (result.data.success !== false) {
                        setMoveHistory([...moveHistory, move.san]);
                        setGame(new Chess(game.fen()));
                        setGameState(prev => ({ ...prev, isMyTurn: false }));
                        setLastMove({ from: selectedSquare, to: square });
                        setSelectedSquare(null);

                        // Check if game ended
                        if (result.data.game_ended) {
                            // Handle game end state
                            let winner = 'Hòa';
                            if (result.data.result === 'white') {
                                winner = gameState.playerColor === 'white' ? gameState.playerName : gameState.botName;
                            } else if (result.data.result === 'black') {
                                winner = gameState.playerColor === 'black' ? gameState.playerName : gameState.botName;
                            }

                            // Format reason message
                            let reasonMsg = result.data.reason;
                            if (result.data.reason === 'insufficient_material') {
                                reasonMsg = 'Không đủ quân để chiếu';
                            } else if (result.data.reason === 'stalemate') {
                                reasonMsg = 'Bí';
                            } else if (result.data.reason === 'checkmate') {
                                reasonMsg = 'Chiếu hết';
                            } else if (result.data.reason === 'fifty_move_rule') {
                                reasonMsg = 'Luật 50 nước đi';
                            }

                            setGameState(prev => ({
                                ...prev,
                                gameStatus: 'finished',
                                winner: winner,
                            }));
                            setError(`Trận đấu kết thúc: ${reasonMsg} - ${winner}`);
                        } else {
                            // Xử lý nước đi của bot nếu có
                            if (result.data.bot_move) {
                                const botGame = new Chess(game.fen());
                                const botMoveObj = botGame.move(result.data.bot_move);
                                setGame(botGame);
                                setMoveHistory(h => [...h, botMoveObj ? botMoveObj.san : result.data.bot_move]);
                                setLastMove({ from: result.data.bot_move.slice(0, 2), to: result.data.bot_move.slice(2, 4) });

                                // ✅ Check game end status from server response
                                if (result.data.status === 'DRAW' || result.data.status === 'WHITE_WIN' || result.data.status === 'BLACK_WIN') {
                                    let winner = 'Hòa';
                                    let reasonMsg = 'Kết thúc';

                                    if (result.data.status === 'WHITE_WIN') {
                                        winner = gameState.playerColor === 'white' ? gameState.playerName : gameState.botName;
                                        reasonMsg = 'Chiếu hết';
                                    } else if (result.data.status === 'BLACK_WIN') {
                                        winner = gameState.playerColor === 'black' ? gameState.playerName : gameState.botName;
                                        reasonMsg = 'Chiếu hết';
                                    } else if (result.data.status === 'DRAW') {
                                        if (botGame.isStalemate()) reasonMsg = 'Bí';
                                        else if (botGame.isInsufficientMaterial()) reasonMsg = 'Không đủ quân để chiếu';
                                        else reasonMsg = 'Hòa';
                                    }

                                    setGameState(prev => ({
                                        ...prev,
                                        gameStatus: 'finished',
                                        winner: winner,
                                        isMyTurn: false,
                                    }));
                                    setError(`Trận đấu kết thúc: ${reasonMsg} - ${winner}`);
                                } else {
                                    setGameState(prev => ({ ...prev, isMyTurn: true }));
                                }
                            } else {
                                setGameState(prev => ({ ...prev, isMyTurn: true }));
                            }
                        }
                    } else {
                        setError(result.data.error || 'Nước đi không hợp lệ');
                    }
                } catch (err) {
                    setError('Lỗi khi đi nước: ' + err.message);
                } finally {
                    setLoading(false);
                }
                return;
            }
            // Nếu click vào quân của mình khác, chọn lại quân
            if (piece && piece.color === (gameState.playerColor === 'white' ? 'w' : 'b')) {
                setSelectedSquare(square);
                return;
            }
            // Nếu click vào ô không hợp lệ, bỏ chọn
            setSelectedSquare(null);
            return;
        }
        // Nếu chưa chọn quân, chọn quân của mình
        if (piece && piece.color === (gameState.playerColor === 'white' ? 'w' : 'b')) {
            setSelectedSquare(square);
        }
    };

    // Đầu hàng
    const handleSurrender = async () => {
        if (!gameState.matchId || gameState.gameStatus !== 'playing') return;
        setLoading(true);
        try {
            await api.post('/game/surrender', {
                match_id: gameState.matchId,
                player_id: gameState.playerId,
            });
            setGameState(prev => ({
                ...prev,
                gameStatus: 'finished',
                winner: gameState.botName,
            }));
        } catch (err) {
            setError('Lỗi đầu hàng: ' + err.message);
        } finally {
            setLoading(false);
        }
    };

    // Reset game
    const handleReset = () => {
        setGame(new Chess());
        setGameState(prev => ({
            ...prev,
            matchId: null,
            gameStatus: 'waiting',
            winner: null,
            isMyTurn: true,
        }));
        setMoveHistory([]);
        setError('');
        setSelectedSquare(null);
        setLastMove(null);
    };

    return (
        <div className="chess-game-container">
            <div className="game-header">
                <h1>🤖 Chơi với Bot</h1>
                <div className="status-bar">
                    {gameState.gameStatus === 'waiting' && (
                        <span className="status waiting">Nhấn bắt đầu để chơi với Bot</span>
                    )}
                    {gameState.gameStatus === 'playing' && (
                        <span className={`status playing ${gameState.isMyTurn ? 'my-turn' : 'opponent-turn'}`}>{gameState.isMyTurn ? 'Lượt của bạn' : 'Bot đang đi...'}</span>
                    )}
                    {gameState.gameStatus === 'finished' && (
                        <span className="status finished">Kết thúc! Thắng: {gameState.winner || 'Hòa'}</span>
                    )}
                    {loading && <span className="status loading">Đang xử lý...</span>}
                </div>
                {error && <div className="error-message">{error}</div>}
            </div>
            <div className="game-content">
                {/* Left Panel */}
                <div className="left-panel">
                    <div className="controls-section">
                        <h3>Thiết lập trận đấu</h3>
                        {!gameState.matchId && (
                            <div className="player-selection">
                                <button onClick={startBotGame} disabled={loading}>Bắt đầu chơi với Bot</button>
                            </div>
                        )}
                        {gameState.matchId && (
                            <div className="game-controls">
                                <button onClick={handleReset}>🔄 Ván mới</button>
                                {gameState.gameStatus === 'playing' && (
                                    <button onClick={handleSurrender} className="surrender-btn">🏳️ Đầu hàng</button>
                                )}
                            </div>
                        )}
                    </div>
                    <div className="game-info">
                        <h3>Thông tin trận đấu</h3>
                        {gameState.matchId && (
                            <>
                                <p><strong>Match ID:</strong> {gameState.matchId}</p>
                                <p><strong>Bạn:</strong> {gameState.playerName} ({gameState.playerColor})</p>
                                <p><strong>Bot:</strong> {gameState.botName}</p>
                                <p><strong>Số nước đi:</strong> {moveHistory.length}</p>
                            </>
                        )}
                        <p><strong>Lượt:</strong> {game.turn() === 'w' ? 'Trắng' : 'Đen'}</p>
                        {game.isCheck() && <p className="check">⚠️ Chiếu tướng!</p>}
                    </div>
                </div>
                {/* Center - Custom Board */}
                <div className="board-container">
                    <div className="board-wrapper">
                        <ChessBoard
                            board={game}
                            selectedSquare={selectedSquare}
                            validMoves={selectedSquare ? getValidMoves(game, selectedSquare) : []}
                            lastMove={lastMove}
                            onSquareClick={handleSquareClick}
                        />
                    </div>
                </div>
                {/* Right Panel */}
                <div className="right-panel">
                    <h3>Lịch sử nước đi</h3>
                    <div className="move-history">
                        {moveHistory.length === 0 ? (
                            <p className="no-moves">Chưa có nước đi nào</p>
                        ) : (
                            <div className="moves-list">
                                {moveHistory.map((move, index) => (
                                    <div key={index} className="move-item">
                                        <span className="move-number">{Math.floor(index / 2) + 1}.</span>
                                        <span className={`move-notation ${index % 2 === 0 ? 'white' : 'black'}`}>{move}</span>
                                    </div>
                                ))}
                            </div>
                        )}
                    </div>
                    <div className="game-state">
                        <h3>Trạng thái</h3>
                        {game.isCheck() && !game.isCheckmate() && (<p className="check">⚠️ Chiếu tướng!</p>)}
                        {game.isCheckmate() && (<p className="checkmate">♔ Chiếu hết! {game.turn() === 'w' ? 'Đen' : 'Trắng'} thắng!</p>)}
                        {game.isDraw() && <p className="draw">🤝 Hòa!</p>}
                        {game.isStalemate() && <p className="stalemate">Hết nước đi!</p>}
                        {game.isThreefoldRepetition() && <p>Ba lần lặp lại</p>}
                        {game.isInsufficientMaterial() && <p>Không đủ quân để thắng</p>}
                    </div>
                </div>
            </div>
        </div>
    );
};

export default ChessGameBot;
