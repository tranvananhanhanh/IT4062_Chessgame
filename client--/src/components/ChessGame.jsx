import { useState, useEffect } from 'react';
import { Chess } from 'chess.js';
import { Chessboard } from 'react-chessboard';
import { gameAPI } from '../api/api';
import './ChessGame.css';

const ChessGame = () => {
    const [game, setGame] = useState(new Chess());
    const [gameState, setGameState] = useState({
        matchId: null,
        playerId: null,
        playerName: '',
        opponentId: null,
        opponentName: '',
        playerColor: null,
        isMyTurn: false,
        gameStatus: 'waiting', // waiting, playing, finished
        winner: null,
    });
    const [moveHistory, setMoveHistory] = useState([]);
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState('');

    // Initialize game for player 1 (white)
    const initializeAsPlayer1 = async () => {
        try {
            setLoading(true);
            setError('');

            const result = await gameAPI.startGame(1, 'alice', 2, 'bob');

            if (result.success) {
                setGameState({
                    matchId: result.match_id,
                    playerId: 1,
                    playerName: 'alice',
                    opponentId: 2,
                    opponentName: 'bob',
                    playerColor: 'white',
                    isMyTurn: true,
                    gameStatus: 'playing',
                    winner: null,
                });

                // Load initial position
                const newGame = new Chess(result.initial_fen);
                setGame(newGame);
                setMoveHistory([]);
            }
        } catch (err) {
            setError('Failed to start game: ' + err.message);
        } finally {
            setLoading(false);
        }
    };

    // Initialize game for player 2 (black)
    const initializeAsPlayer2 = async () => {
        try {
            setLoading(true);
            setError('');

            // For demo, assume match ID 1 exists
            const result = await gameAPI.joinGame(1, 2, 'bob');

            if (result.success) {
                setGameState({
                    matchId: result.match_id,
                    playerId: 2,
                    playerName: 'bob',
                    opponentId: 1,
                    opponentName: 'alice',
                    playerColor: 'black',
                    isMyTurn: false,
                    gameStatus: 'playing',
                    winner: null,
                });

                // Load initial position
                const newGame = new Chess(result.initial_fen);
                setGame(newGame);
                setMoveHistory([]);
            }
        } catch (err) {
            setError('Failed to join game: ' + err.message);
        } finally {
            setLoading(false);
        }
    };

    // Handle piece drop (make move)
    const onDrop = async (sourceSquare, targetSquare) => {
        if (!gameState.isMyTurn || gameState.gameStatus !== 'playing') {
            return false;
        }

        try {
            // Validate move locally first
            const gameCopy = new Chess(game.fen());
            const move = gameCopy.move({
                from: sourceSquare,
                to: targetSquare,
                promotion: 'q'
            });

            if (!move) {
                return false;
            }

            // Send move to server
            setLoading(true);
            const result = await gameAPI.makeMove(
                gameState.matchId,
                gameState.playerId,
                sourceSquare,
                targetSquare
            );

            if (result.success) {
                // Update local game state
                setGame(gameCopy);
                setMoveHistory(gameCopy.history());

                // Update game state
                setGameState(prev => ({
                    ...prev,
                    isMyTurn: false,
                    gameStatus: result.game_status === 'finished' ? 'finished' : 'playing',
                }));

                return true;
            } else {
                setError(result.error || 'Invalid move');
                return false;
            }
        } catch (err) {
            setError('Failed to make move: ' + err.message);
            return false;
        } finally {
            setLoading(false);
        }
    };

    // Surrender game
    const handleSurrender = async () => {
        if (!gameState.matchId || gameState.gameStatus !== 'playing') {
            return;
        }

        try {
            setLoading(true);
            await gameAPI.surrenderGame(gameState.matchId, gameState.playerId);

            setGameState(prev => ({
                ...prev,
                gameStatus: 'finished',
                winner: prev.opponentName,
            }));
        } catch (err) {
            setError('Failed to surrender: ' + err.message);
        } finally {
            setLoading(false);
        }
    };

    // Reset game
    const handleReset = () => {
        setGame(new Chess());
        setGameState({
            matchId: null,
            playerId: null,
            playerName: '',
            opponentId: null,
            opponentName: '',
            playerColor: null,
            isMyTurn: false,
            gameStatus: 'waiting',
            winner: null,
        });
        setMoveHistory([]);
        setError('');
    };

    return (
        <div className="chess-game-container">
            <div className="game-header">
                <h1>♔ Online Chess Game ♚</h1>
                <div className="status-bar">
                    {gameState.gameStatus === 'waiting' && (
                        <span className="status waiting">Choose your role to start playing</span>
                    )}
                    {gameState.gameStatus === 'playing' && (
                        <span className={`status playing ${gameState.isMyTurn ? 'my-turn' : 'opponent-turn'}`}>
                            {gameState.isMyTurn ? 'Your turn' : `${gameState.opponentName}'s turn`}
                        </span>
                    )}
                    {gameState.gameStatus === 'finished' && (
                        <span className="status finished">
                            Game Over! Winner: {gameState.winner || 'Draw'}
                        </span>
                    )}
                    {loading && <span className="status loading">Processing...</span>}
                </div>
                {error && <div className="error-message">{error}</div>}
            </div>

            <div className="game-content">
                {/* Left Panel */}
                <div className="left-panel">
                    <div className="controls-section">
                        <h3>Game Setup</h3>
                        {!gameState.matchId && (
                            <div className="player-selection">
                                <button onClick={initializeAsPlayer1} disabled={loading}>
                                    Play as White (Alice)
                                </button>
                                <button onClick={initializeAsPlayer2} disabled={loading}>
                                    Play as Black (Bob)
                                </button>
                            </div>
                        )}
                        {gameState.matchId && (
                            <div className="game-controls">
                                <button onClick={handleReset}>
                                    🔄 New Game
                                </button>
                                {gameState.gameStatus === 'playing' && (
                                    <button onClick={handleSurrender} className="surrender-btn">
                                        🏳️ Surrender
                                    </button>
                                )}
                            </div>
                        )}
                    </div>

                    <div className="game-info">
                        <h3>Game Info</h3>
                        {gameState.matchId && (
                            <>
                                <p><strong>Match ID:</strong> {gameState.matchId}</p>
                                <p><strong>You:</strong> {gameState.playerName} ({gameState.playerColor})</p>
                                <p><strong>Opponent:</strong> {gameState.opponentName}</p>
                                <p><strong>Moves:</strong> {moveHistory.length}</p>
                            </>
                        )}
                        <p><strong>Turn:</strong> {game.turn() === 'w' ? 'White' : 'Black'}</p>
                        {game.isCheck() && <p className="check">⚠️ Check!</p>}
                    </div>
                </div>

                {/* Center - Chessboard */}
                <div className="board-container">
                    <div className="board-wrapper">
                        <Chessboard
                            position={game.fen()}
                            onPieceDrop={onDrop}
                            boardOrientation={gameState.playerColor === 'black' ? 'black' : 'white'}
                            customBoardStyle={{
                                borderRadius: '5px',
                                boxShadow: '0 5px 15px rgba(0, 0, 0, 0.5)'
                            }}
                            customDarkSquareStyle={{ backgroundColor: '#769656' }}
                            customLightSquareStyle={{ backgroundColor: '#eeeed2' }}
                            isDraggablePiece={({ piece, sourceSquare }) => {
                                // Only allow dragging if it's player's turn and piece belongs to player
                                if (!gameState.isMyTurn || gameState.gameStatus !== 'playing') {
                                    return false;
                                }

                                const pieceColor = piece[0];
                                const playerColor = gameState.playerColor === 'white' ? 'w' : 'b';
                                return pieceColor === playerColor;
                            }}
                        />
                    </div>
                </div>

                {/* Right Panel */}
                <div className="right-panel">
                    <h3>Move History</h3>
                    <div className="move-history">
                        {moveHistory.length === 0 ? (
                            <p className="no-moves">No moves yet</p>
                        ) : (
                            <div className="moves-list">
                                {moveHistory.map((move, index) => (
                                    <div key={index} className="move-item">
                                        <span className="move-number">{Math.floor(index / 2) + 1}.</span>
                                        <span className={`move-notation ${index % 2 === 0 ? 'white' : 'black'}`}>
                                            {move}
                                        </span>
                                    </div>
                                ))}
                            </div>
                        )}
                    </div>

                    <div className="game-state">
                        <h3>Status</h3>
                        {game.isCheck() && !game.isCheckmate() && (
                            <p className="check">⚠️ King is in Check!</p>
                        )}
                        {game.isCheckmate() && (
                            <p className="checkmate">
                                ♔ Checkmate! {game.turn() === 'w' ? 'Black' : 'White'} wins!
                            </p>
                        )}
                        {game.isDraw() && <p className="draw">🤝 Game is a Draw!</p>}
                        {game.isStalemate() && <p className="stalemate">Stalemate!</p>}
                        {game.isThreefoldRepetition() && <p>Threefold Repetition</p>}
                        {game.isInsufficientMaterial() && <p>Insufficient Material</p>}
                    </div>
                </div>
            </div>
        </div>
    );
};

export default ChessGame;