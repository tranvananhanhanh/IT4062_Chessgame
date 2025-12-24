import { useState, useEffect } from 'react';
import { Chess } from 'chess.js';
import api, { gameAPI } from '../../api/api';
import ChessBoard from '../chess/ChessBoard';
import GameControl from '../gamecontrol/GameControl';
import './ChessGame.css';

const getValidMoves = (game, selected) => {
    if (!selected) return [];
    const moves = game.moves({ square: selected, verbose: true });
    return moves.map((m) => m.to);
};

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
        gameStatus: 'waiting', // waiting, playing, paused, finished
        winner: null,
    });
    const [moveHistory, setMoveHistory] = useState([]);
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState('');
    const [selectedSquare, setSelectedSquare] = useState(null);
    const [lastMove, setLastMove] = useState(null);
    const [showControl, setShowControl] = useState(false);
    const [joinMatchId, setJoinMatchId] = useState('');

    // Poll for game state updates (opponent actions, status changes)
    useEffect(() => {
        if (!gameState.matchId || gameState.gameStatus === 'waiting') return;

        console.log('[Polling] Starting poll for match', gameState.matchId);

        const pollInterval = setInterval(async () => {
            try {
                const status = await gameAPI.getMatchStatus(gameState.matchId);

                if (!status.success) {
                    console.log('[Polling] Failed to get status');
                    return;
                }

                console.log('[Polling] Status:', status);

                // Sync game status (PLAYING/PAUSED/FINISHED)
                if (status.status === 'PLAYING' && gameState.gameStatus === 'paused') {
                    console.log('[Polling] Game resumed by opponent');
                    setGameState(prev => ({ ...prev, gameStatus: 'playing' }));
                    setError('▶️ Opponent resumed the game');
                } else if (status.status === 'PAUSED' && gameState.gameStatus === 'playing') {
                    console.log('[Polling] Game paused by opponent');
                    setGameState(prev => ({ ...prev, gameStatus: 'paused' }));
                    setError('⏸️ Opponent paused the game');
                } else if (status.status === 'FINISHED' && gameState.gameStatus !== 'finished') {
                    console.log('[Polling] Game finished');
                    setGameState(prev => ({ ...prev, gameStatus: 'finished' }));
                    setError('🏁 Game ended - Draw accepted!');
                }

                // ✅ Check for rematch acceptance (new match created) - from database query
                if (status.new_match_id && status.new_match_id !== gameState.matchId) {
                    console.log('[Polling] Rematch accepted! New match:', status.new_match_id);

                    const newGame = new Chess();
                    const newPlayerColor = gameState.playerColor === 'white' ? 'black' : 'white';
                    const newIsMyTurn = newPlayerColor === 'white';

                    // ✅ IMPORTANT: Join the new match so C server knows about this connection
                    (async () => {
                        try {
                            await gameAPI.joinGame(
                                status.new_match_id,
                                gameState.playerId,
                                gameState.playerName
                            );
                            console.log('[Rematch] Successfully joined new match', status.new_match_id);
                        } catch (err) {
                            console.error('[Rematch] Failed to join new match:', err);
                        }
                    })();

                    setGame(newGame);
                    setGameState({
                        matchId: status.new_match_id,
                        playerId: gameState.playerId,
                        playerName: gameState.playerName,
                        opponentId: gameState.opponentId,
                        opponentName: gameState.opponentName,
                        playerColor: newPlayerColor,
                        isMyTurn: newIsMyTurn,
                        gameStatus: 'playing',
                        winner: null,
                    });
                    setMoveHistory([]);
                    setLastMove(null);
                    setSelectedSquare(null);
                    setError(`✅ Opponent accepted rematch! New Match ID: ${status.new_match_id}. You are now playing as ${newPlayerColor}.`);

                    // Stop polling the old match
                    return;
                }

                // Check for FEN changes (opponent made a move)
                const currentFen = game.fen();
                if (status.current_fen && status.current_fen !== currentFen) {
                    console.log('[Polling] Board state changed!');
                    console.log('[Polling] Current FEN:', currentFen);
                    console.log('[Polling] New FEN:', status.current_fen);

                    const newGame = new Chess(status.current_fen);
                    setGame(newGame);

                    // Update whose turn it is
                    const isMyTurn = (newGame.turn() === 'w' && gameState.playerColor === 'white') ||
                        (newGame.turn() === 'b' && gameState.playerColor === 'black');

                    setGameState(prev => ({ ...prev, isMyTurn }));

                    if (isMyTurn) {
                        setError('✅ Opponent moved. Your turn!');
                    } else {
                        setError(`⏳ Waiting for opponent...`);
                    }
                }

            } catch (err) {
                console.error('[Polling] Error:', err);
            }
        }, 2000); // Poll every 2 seconds

        return () => {
            console.log('[Polling] Stopping poll for match', gameState.matchId);
            clearInterval(pollInterval);
        };
    }, [gameState.matchId, gameState.gameStatus, gameState.playerColor]);

    // Separate effect for waiting state (checking if opponent joined)
    useEffect(() => {
        if (gameState.gameStatus !== 'waiting' || !gameState.matchId) return;

        console.log('[Polling] Waiting for opponent to join match', gameState.matchId);

        const pollInterval = setInterval(async () => {
            try {
                const status = await gameAPI.getMatchStatus(gameState.matchId);

                if (!status.success) return;

                // Check if game status changed to PLAYING (opponent joined)
                if (status.status === 'PLAYING' || status.black_online) {
                    console.log('[Polling] Opponent joined! Game status:', status.status);
                    setGameState(prev => ({
                        ...prev,
                        gameStatus: 'playing',
                        opponentId: 2,
                        opponentName: 'bob'
                    }));
                    setError('✅ Opponent joined! Game started.');
                }

            } catch (err) {
                console.error('[Polling] Error checking opponent join:', err);
            }
        }, 2000);

        return () => clearInterval(pollInterval);
    }, [gameState.matchId, gameState.gameStatus]);

    // Player 1 creates a new match as White
    const initializeAsPlayer1 = async () => {
        try {
            setLoading(true);
            setError('');

            // Create match with Player 1 as white, Player 2 (bob) as placeholder
            const result = await gameAPI.startGame(1, 'alice', 2, 'bob');

            if (result.success) {
                setGameState({
                    matchId: result.match_id,
                    playerId: 1,
                    playerName: 'alice',
                    opponentId: null,
                    opponentName: 'Waiting...',
                    playerColor: 'white',
                    isMyTurn: true,
                    gameStatus: 'waiting', // Still waiting for player 2 to join
                    winner: null,
                });

                const newGame = new Chess(result.initial_fen);
                setGame(newGame);
                setMoveHistory([]);

                setError(`✅ Match created! Share Match ID: ${result.match_id} with your opponent.`);
            }
        } catch (err) {
            setError('Failed to start game: ' + err.message);
        } finally {
            setLoading(false);
        }
    };

    // Player 2 joins an existing match
    const handleJoinMatch = async () => {
        try {
            if (!joinMatchId || isNaN(joinMatchId)) {
                setError('Please enter a valid Match ID');
                return;
            }

            setLoading(true);
            setError('');

            const matchId = parseInt(joinMatchId);
            const result = await gameAPI.joinGame(matchId, 2, 'bob');

            if (result.success) {
                setGameState({
                    matchId: matchId,
                    playerId: 2,
                    playerName: 'bob',
                    opponentId: 1,
                    opponentName: 'alice',
                    playerColor: 'black',
                    isMyTurn: false,
                    gameStatus: 'playing',
                    winner: null,
                });

                const newGame = new Chess(result.initial_fen);
                setGame(newGame);
                setMoveHistory([]);

                setError('✅ Successfully joined the match!');
            }
        } catch (err) {
            setError('Failed to join game: ' + (err.response?.data?.error || err.message));
        } finally {
            setLoading(false);
        }
    };

    const handleSquareClick = async (row, col, square) => {
        if (gameState.gameStatus === 'paused') {
            setError('⏸️ Game is paused. Click Resume to continue.');
            return;
        }

        if (!gameState.isMyTurn || gameState.gameStatus !== 'playing') return;

        const piece = game.get(square);

        if (selectedSquare) {
            if (selectedSquare === square) {
                setSelectedSquare(null);
                return;
            }
            const validMoves = getValidMoves(game, selectedSquare);
            if (validMoves.includes(square)) {
                setLoading(true);
                try {
                    const move = game.move({ from: selectedSquare, to: square, promotion: 'q' });
                    if (!move) {
                        setError('Invalid move');
                        setLoading(false);
                        return;
                    }

                    const result = await gameAPI.makeMove(
                        gameState.matchId,
                        gameState.playerId,
                        selectedSquare,
                        square
                    );

                    if (result.success) {
                        setMoveHistory([...moveHistory, move.san]);
                        setGame(new Chess(game.fen()));
                        setGameState(prev => ({
                            ...prev,
                            isMyTurn: false,
                            gameStatus: result.game_status === 'finished' ? 'finished' : 'playing',
                        }));
                        setLastMove({ from: selectedSquare, to: square });
                        setSelectedSquare(null);
                    } else {
                        setError(result.error || 'Invalid move');
                    }
                } catch (err) {
                    setError('Failed to make move: ' + (err.response?.data?.error || err.message));
                } finally {
                    setLoading(false);
                }
                return;
            }

            if (piece && piece.color === (gameState.playerColor === 'white' ? 'w' : 'b')) {
                setSelectedSquare(square);
                return;
            }
            setSelectedSquare(null);
            return;
        }

        if (piece && piece.color === (gameState.playerColor === 'white' ? 'w' : 'b')) {
            setSelectedSquare(square);
        }
    };

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
                    {gameState.gameStatus === 'paused' && (
                        <span className="status paused">⏸️ Game Paused</span>
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

                <div className="left-panel" style={{ position: 'relative' }}>
                    <div className="controls-section">
                        <h3>Game Setup</h3>
                        {!gameState.matchId && (
                            <div className="player-selection">
                                <button
                                    onClick={initializeAsPlayer1}
                                    disabled={loading}
                                    style={{ marginBottom: '1rem' }}
                                >
                                    ♔ Play as White (Alice)
                                </button>

                                <div style={{ margin: '1.5rem 0', padding: '1rem', background: '#f0f0f0', borderRadius: '8px' }}>
                                    <p style={{ marginBottom: '0.5rem', fontWeight: 'bold' }}>Join Existing Match</p>
                                    <input
                                        type="text"
                                        placeholder="Enter Match ID"
                                        value={joinMatchId}
                                        onChange={(e) => setJoinMatchId(e.target.value)}
                                        style={{
                                            width: '100%',
                                            padding: '0.5rem',
                                            marginBottom: '0.5rem',
                                            borderRadius: '4px',
                                            border: '1px solid #ccc'
                                        }}
                                    />
                                    <button
                                        onClick={handleJoinMatch}
                                        disabled={loading || !joinMatchId}
                                        style={{ width: '100%' }}
                                    >
                                        ♚ Join as Black (Bob)
                                    </button>
                                </div>
                            </div>
                        )}
                        {gameState.matchId && (
                            <div className="game-controls">
                                {gameState.gameStatus === 'waiting' && (
                                    <div style={{ padding: '1rem', background: '#fff3cd', borderRadius: '8px', marginBottom: '1rem' }}>
                                        <p style={{ fontWeight: 'bold', color: '#856404' }}>
                                            ⏳ Waiting for opponent...
                                        </p>
                                        <p style={{ fontSize: '0.9rem', color: '#856404' }}>
                                            Share Match ID: <strong>{gameState.matchId}</strong>
                                        </p>
                                    </div>
                                )}
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
                                <p><strong>Status:</strong> {gameState.gameStatus}</p>
                                <p><strong>Moves:</strong> {moveHistory.length}</p>
                            </>
                        )}
                        {gameState.matchId && (
                            <p><strong>Turn:</strong> {game.turn() === 'w' ? 'White' : 'Black'}</p>
                        )}
                        {game.isCheck() && <p className="check">⚠️ Check!</p>}
                    </div>
                </div>

                <div className="board-container">
                    <div className="board-wrapper" style={{ position: 'relative' }}>

                        <ChessBoard
                            board={game}
                            selectedSquare={selectedSquare}
                            validMoves={selectedSquare ? getValidMoves(game, selectedSquare) : []}
                            lastMove={lastMove}
                            onSquareClick={handleSquareClick}
                        />

                        {/* ✅ Paused Overlay - CHỈ nằm trong board-wrapper */}
                        {gameState.gameStatus === 'paused' && (
                            <div
                                style={{
                                    position: 'absolute',
                                    top: 0,
                                    left: 0,
                                    right: 0,
                                    bottom: 0,
                                    background: 'rgba(0, 0, 0, 0.7)',
                                    backdropFilter: 'blur(4px)',
                                    display: 'flex',
                                    alignItems: 'center',
                                    justifyContent: 'center',
                                    zIndex: 5,
                                    pointerEvents: 'auto',
                                }}
                                onClick={(e) => {
                                    e.stopPropagation();
                                    setError('⏸️ Game is paused. Click Resume to continue.');
                                }}
                            >
                                <div className="paused-message">
                                    <h2>⏸️ Game Paused</h2>
                                    <p>Click Resume in Game Control to continue</p>
                                </div>
                            </div>
                        )}
                    </div>
                </div>

                <div className="right-panel" style={{ position: 'relative' }}>
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

                    {/* GameControl inside right panel (absolute positioned) */}
                    <div style={{ position: 'absolute', bottom: '1.5rem', right: '1rem', zIndex: 200 }}>
                        <button
                            className="toggle-control-btn"
                            onClick={() => setShowControl((v) => !v)}
                            style={{ zIndex: 210 }}
                        >
                            {showControl ? 'Đóng Game Control' : '⚙️ Game Control'}
                        </button>

                        <div
                            className="game-control-slide"
                            style={{
                                position: 'absolute',
                                bottom: '3.2rem',
                                right: showControl ? '0' : '-320px',
                                width: '260px',
                                transition: 'right 0.3s',
                                zIndex: 200,
                            }}
                        >
                            <GameControl
                                matchId={gameState.matchId}
                                playerId={gameState.playerId}
                                gameStatus={gameState.gameStatus}
                                onControlResult={(action, res) => {
                                    if (res.success) {
                                        if (action === 'PAUSE') {
                                            setGameState(prev => ({ ...prev, gameStatus: 'paused' }));
                                            setError('⏸️ Game paused');
                                        } else if (action === 'RESUME') {
                                            setGameState(prev => ({ ...prev, gameStatus: 'playing' }));
                                            setError('▶️ Game resumed');
                                        } else if (action === 'DRAW') {
                                            setError('🤝 Draw offer sent to opponent');
                                        } else if (action === 'DRAW_ACCEPT') {
                                            setGameState(prev => ({ ...prev, gameStatus: 'finished' }));
                                            setError('🤝 Draw accepted - Game ended');
                                        } else if (action === 'REMATCH') {
                                            setError('🔄 Rematch request sent to opponent');
                                        } else if (action === 'REMATCH_ACCEPT' && res.new_match_id) {
                                            // ✅ Tự động chuyển sang ván mới
                                            const newMatchId = res.new_match_id;
                                            const newGame = new Chess();

                                            // Swap colors for rematch
                                            const newPlayerColor = gameState.playerColor === 'white' ? 'black' : 'white';
                                            const newIsMyTurn = newPlayerColor === 'white';

                                            // ✅ IMPORTANT: Join the new match so C server knows about this connection
                                            (async () => {
                                                try {
                                                    await gameAPI.joinGame(
                                                        newMatchId,
                                                        gameState.playerId,
                                                        gameState.playerName
                                                    );
                                                    console.log('[Rematch] Acceptor joined new match', newMatchId);
                                                } catch (err) {
                                                    console.error('[Rematch] Acceptor failed to join new match:', err);
                                                }
                                            })();

                                            setGame(newGame);
                                            setGameState({
                                                matchId: newMatchId,
                                                playerId: gameState.playerId,
                                                playerName: gameState.playerName,
                                                opponentId: gameState.opponentId,
                                                opponentName: gameState.opponentName,
                                                playerColor: newPlayerColor,
                                                isMyTurn: newIsMyTurn,
                                                gameStatus: 'playing',
                                                winner: null,
                                            });
                                            setMoveHistory([]);
                                            setLastMove(null);
                                            setSelectedSquare(null);
                                            setError(`✅ Rematch started! New Match ID: ${newMatchId}. You are ${newPlayerColor}.`);
                                        }
                                    }
                                }}
                            />
                        </div>
                    </div>

                </div>
            </div>
        </div>
    );
};

export default ChessGame;
