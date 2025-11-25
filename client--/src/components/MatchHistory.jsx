import { useState, useEffect } from 'react';
import { historyAPI } from '../api/api';
import './MatchHistory.css';

const MatchHistory = () => {
    const [userId, setUserId] = useState(1);
    const [history, setHistory] = useState([]);
    const [stats, setStats] = useState(null);
    const [selectedMatch, setSelectedMatch] = useState(null);
    const [replay, setReplay] = useState(null);
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState('');

    // Load history and stats when userId changes
    useEffect(() => {
        loadHistoryAndStats();
    }, [userId]);

    const loadHistoryAndStats = async () => {
        try {
            setLoading(true);
            setError('');

            // Load history
            const historyResult = await historyAPI.getGameHistory(userId);
            if (historyResult.success) {
                setHistory(historyResult.history.matches || []);
            }

            // Load stats
            const statsResult = await historyAPI.getPlayerStats(userId);
            if (statsResult.success) {
                setStats(statsResult.stats);
            }
        } catch (err) {
            setError('Failed to load data: ' + err.message);
        } finally {
            setLoading(false);
        }
    };

    const loadReplay = async (matchId) => {
        try {
            setLoading(true);
            const result = await historyAPI.getGameReplay(matchId);
            if (result.success) {
                setReplay(result.replay);
                setSelectedMatch(matchId);
            }
        } catch (err) {
            setError('Failed to load replay: ' + err.message);
        } finally {
            setLoading(false);
        }
    };

    const getResultIcon = (result) => {
        switch (result) {
            case 'win': return '🏆';
            case 'loss': return '❌';
            case 'draw': return '🤝';
            default: return '❓';
        }
    };

    const getResultClass = (result) => {
        switch (result) {
            case 'win': return 'win';
            case 'loss': return 'loss';
            case 'draw': return 'draw';
            default: return '';
        }
    };

    return (
        <div className="match-history-container">
            <div className="history-header">
                <h1>📊 Match History & Statistics</h1>

                <div className="user-selector">
                    <label>Select User:</label>
                    <select
                        value={userId}
                        onChange={(e) => setUserId(parseInt(e.target.value))}
                    >
                        <option value={1}>Alice (ID: 1)</option>
                        <option value={2}>Bob (ID: 2)</option>
                    </select>
                </div>
            </div>

            {error && <div className="error-message">{error}</div>}

            <div className="history-content">
                {/* Statistics Panel */}
                <div className="stats-panel">
                    <h2>Player Statistics</h2>
                    {stats ? (
                        <div className="stats-grid">
                            <div className="stat-item">
                                <span className="stat-label">Total Games:</span>
                                <span className="stat-value">{stats.total_games}</span>
                            </div>
                            <div className="stat-item">
                                <span className="stat-label">Wins:</span>
                                <span className="stat-value win">{stats.wins}</span>
                            </div>
                            <div className="stat-item">
                                <span className="stat-label">Losses:</span>
                                <span className="stat-value loss">{stats.losses}</span>
                            </div>
                            <div className="stat-item">
                                <span className="stat-label">Draws:</span>
                                <span className="stat-value draw">{stats.draws}</span>
                            </div>
                            <div className="stat-item">
                                <span className="stat-label">Win Rate:</span>
                                <span className="stat-value">{stats.win_rate}%</span>
                            </div>
                            <div className="stat-item">
                                <span className="stat-label">Current ELO:</span>
                                <span className="stat-value">{stats.current_elo}</span>
                            </div>
                        </div>
                    ) : (
                        <p>Loading statistics...</p>
                    )}
                </div>

                {/* Match History */}
                <div className="matches-panel">
                    <h2>Match History</h2>
                    {loading ? (
                        <p>Loading matches...</p>
                    ) : history.length === 0 ? (
                        <p>No matches found for this user.</p>
                    ) : (
                        <div className="matches-list">
                            {history.map((match) => (
                                <div
                                    key={match.match_id}
                                    className={`match-item ${selectedMatch === match.match_id ? 'selected' : ''}`}
                                    onClick={() => loadReplay(match.match_id)}
                                >
                                    <div className="match-header">
                                        <span className="match-id">Match #{match.match_id}</span>
                                        <span className={`match-result ${getResultClass(match.result)}`}>
                                            {getResultIcon(match.result)} {match.result.toUpperCase()}
                                        </span>
                                    </div>
                                    <div className="match-details">
                                        <span>vs {match.opponent_name}</span>
                                        <span>({match.player_color})</span>
                                        <span>{match.move_count} moves</span>
                                    </div>
                                    <div className="match-times">
                                        <span>Started: {new Date(match.start_time).toLocaleString()}</span>
                                        {match.end_time && (
                                            <span>Ended: {new Date(match.end_time).toLocaleString()}</span>
                                        )}
                                    </div>
                                </div>
                            ))}
                        </div>
                    )}
                </div>

                {/* Replay Panel */}
                {replay && (
                    <div className="replay-panel">
                        <h2>Game Replay - Match #{selectedMatch}</h2>
                        <div className="replay-info">
                            <p><strong>Total Moves:</strong> {replay.move_count}</p>
                        </div>
                        <div className="replay-moves">
                            {replay.moves && replay.moves.length > 0 ? (
                                <div className="moves-grid">
                                    {replay.moves.map((move) => (
                                        <div key={move.move_number} className="replay-move">
                                            <span className="move-number">{move.move_number}.</span>
                                            <span className="move-notation">{move.notation}</span>
                                            <span className="move-fen" title={move.fen_after}>
                                                Position after move
                                            </span>
                                        </div>
                                    ))}
                                </div>
                            ) : (
                                <p>No moves recorded for this match.</p>
                            )}
                        </div>
                    </div>
                )}
            </div>
        </div>
    );
};

export default MatchHistory;