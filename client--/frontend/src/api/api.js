import axios from 'axios';

const API_BASE_URL = 'http://localhost:5000/api';

// Create axios instance with default config
const api = axios.create({
    baseURL: API_BASE_URL,
    headers: {
        'Content-Type': 'application/json',
    },
});

// Game API functions
export const gameAPI = {
    // Start a new game
    startGame: async (player1Id, player1Name, player2Id, player2Name) => {
        try {
            const response = await api.post('/game/start', {
                player1_id: player1Id,
                player1_name: player1Name,
                player2_id: player2Id,
                player2_name: player2Name,
            });
            return response.data;
        } catch (error) {
            console.error('Error starting game:', error);
            throw error;
        }
    },

    // Make a move
    makeMove: async (matchId, playerId, fromSquare, toSquare) => {
        try {
            const response = await api.post('/game/move', {
                match_id: matchId,
                player_id: playerId,
                from_square: fromSquare,
                to_square: toSquare,
            });
            return response.data;
        } catch (error) {
            console.error('Error making move:', error);
            throw error;
        }
    },

    // Join an existing game
    joinGame: async (matchId, playerId, playerName) => {
        try {
            const response = await api.post(`/game/join/${matchId}`, {
                player_id: playerId,
                player_name: playerName,
            });
            return response.data;
        } catch (error) {
            console.error('Error joining game:', error);
            throw error;
        }
    },

    // Surrender game
    surrenderGame: async (matchId, playerId) => {
        try {
            const response = await api.post('/game/surrender', {
                match_id: matchId,
                player_id: playerId,
            });
            return response.data;
        } catch (error) {
            console.error('Error surrendering game:', error);
            throw error;
        }
    },
};

// History API functions
export const historyAPI = {
    // Get game history for a user
    getGameHistory: async (userId) => {
        try {
            const response = await api.get(`/history/${userId}`);
            return response.data;
        } catch (error) {
            console.error('Error getting game history:', error);
            throw error;
        }
    },

    // Get detailed replay of a match
    getGameReplay: async (matchId) => {
        try {
            const response = await api.get(`/history/replay/${matchId}`);
            return response.data;
        } catch (error) {
            console.error('Error getting game replay:', error);
            throw error;
        }
    },

    // Get player statistics
    getPlayerStats: async (userId) => {
        try {
            const response = await api.get(`/stats/${userId}`);
            return response.data;
        } catch (error) {
            console.error('Error getting player stats:', error);
            throw error;
        }
    },
};

// Health check
export const healthAPI = {
    checkHealth: async () => {
        try {
            const response = await api.get('/health');
            return response.data;
        } catch (error) {
            console.error('Health check failed:', error);
            throw error;
        }
    },
};

export default api;