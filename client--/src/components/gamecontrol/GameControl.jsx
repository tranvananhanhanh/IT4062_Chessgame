import React, { useState } from 'react';
import api from '../../api/api';
import './GameControl.css';

const actions = [
    { key: 'PAUSE', label: '⏸ Tạm dừng' },
    { key: 'RESUME', label: '▶️ Tiếp tục' },
    { key: 'DRAW', label: '🤝 Xin hòa' },
    { key: 'DRAW_ACCEPT', label: '✅ Chấp nhận hòa' },
    { key: 'DRAW_DECLINE', label: '❌ Từ chối hòa' },
    { key: 'REMATCH', label: '🔄 Yêu cầu đấu lại' },
    { key: 'REMATCH_ACCEPT', label: '✅ Chấp nhận đấu lại' },
    { key: 'REMATCH_DECLINE', label: '❌ Từ chối đấu lại' },
];

function GameControl({ matchId, playerId, gameStatus, onControlResult }) {
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState('');
    const [success, setSuccess] = useState('');
    const [selectedAction, setSelectedAction] = useState('');

    const isDisabled = (actionKey) => {
        if (loading || !matchId) return true;
        switch (actionKey) {
            case 'PAUSE':
                return gameStatus !== 'playing';
            case 'RESUME':
                return gameStatus !== 'paused';
            case 'DRAW':
                return gameStatus !== 'playing';
            case 'DRAW_ACCEPT':
            case 'DRAW_DECLINE':
                // accept/decline only meaningful when a draw offer exists; allow when playing/paused
                return gameStatus === 'waiting' || gameStatus === 'finished';
            case 'REMATCH':
                return gameStatus !== 'finished';
            case 'REMATCH_ACCEPT':
            case 'REMATCH_DECLINE':
                return gameStatus !== 'finished';
            default:
                return false;
        }
    };

    const handleAction = async (action) => {
        setSelectedAction(action); // highlight pressed button
        setLoading(true);
        setError('');
        setSuccess('');
        try {
            const response = await api.post('/game/control', {
                match_id: matchId,
                player_id: playerId,
                action,
            });
            if (response.data.success) {
                setSuccess(response.data.message || 'Thành công!');
                if (onControlResult) onControlResult(action, response.data);
            } else {
                setError(response.data.error || 'Thao tác thất bại!');
            }
        } catch (err) {
            setError(err.response?.data?.error || err.message || 'Lỗi kết nối server!');
        } finally {
            setLoading(false);
        }
    };

    return (
        <div className="game-control-panel">
            <h4>Game Control</h4>
            <div className="control-actions">
                {actions.map(({ key, label }) => (
                    <button
                        key={key}
                        className={`control-btn${selectedAction === key ? ' selected' : ''}`}
                        disabled={isDisabled(key)}
                        onClick={() => handleAction(key)}
                    >
                        {label}
                    </button>
                ))}
            </div>
            {loading && <div className="control-status loading">Đang xử lý...</div>}
            {error && <div className="control-status error">{error}</div>}
            {success && <div className="control-status success">{success}</div>}
        </div>
    );
}

export default GameControl;
