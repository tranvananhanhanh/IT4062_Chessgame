export const API_BASE_URL = 'http://localhost:5000/api';

const api = {
    async login(username, password) {
        const response = await fetch(`${API_BASE_URL}/auth/login`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username, password }),
        });
        const data = await response.json();
        if (!response.ok) throw new Error(data.message || 'Đăng nhập thất bại');
        return data; // { token, user }
    },

    async makeMove(token, gameId, move) {
        const response = await fetch(`${API_BASE_URL}/game/move`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${token}`,
            },
            body: JSON.stringify({ game_id: gameId, move }),
        });
        const data = await response.json();
        if (!response.ok) throw new Error(data.message || 'Lỗi khi thực hiện nước đi');
        return data;
    },

    async getHistory(_token) {
        // Mock dữ liệu lịch sử
        return [
            { id: 1, date: '2025-01-01', result: 'Thắng', opponent: 'AI Level 3' },
            { id: 2, date: '2025-01-02', result: 'Thua', opponent: 'AI Level 3' },
        ];
    },
};

export default api;
