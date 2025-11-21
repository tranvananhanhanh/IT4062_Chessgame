from flask import Flask, request, jsonify
from flask_cors import CORS
from services.game_service import get_game_service
from services.history_service import get_history_service

app = Flask(__name__)
CORS(app)  # Enable CORS for React frontend

# Initialize services
game_service = get_game_service()
history_service = get_history_service()

@app.route('/api/health', methods=['GET'])
def health_check():
    """Health check endpoint"""
    return jsonify({"status": "ok", "message": "Flask backend is running"})

@app.route('/api/game/start', methods=['POST'])
def start_game():
    """
    Start a new 1v1 game
    Expected JSON: {"player1_id": 1, "player1_name": "alice", "player2_id": 2, "player2_name": "bob"}
    """
    try:
        data = request.get_json()
        player1_id = data.get('player1_id')
        player1_name = data.get('player1_name')
        player2_id = data.get('player2_id')
        player2_name = data.get('player2_name')

        if not all([player1_id, player1_name, player2_id, player2_name]):
            return jsonify({"error": "Missing required fields"}), 400

        result = game_service.start_game(player1_id, player1_name, player2_id, player2_name)

        if result["success"]:
            return jsonify(result)
        else:
            return jsonify({"error": result["error"]}), 500

    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/game/move', methods=['POST'])
def make_move():
    """
    Make a move in the game
    Expected JSON: {"match_id": 1, "player_id": 1, "from_square": "e2", "to_square": "e4"}
    """
    try:
        data = request.get_json()
        match_id = data.get('match_id')
        player_id = data.get('player_id')
        from_square = data.get('from_square')
        to_square = data.get('to_square')

        if not all([match_id, player_id, from_square, to_square]):
            return jsonify({"error": "Missing required fields"}), 400

        result = game_service.make_move(match_id, player_id, from_square, to_square)

        if result["success"]:
            return jsonify(result)
        else:
            return jsonify({"error": result["error"]}), 400

    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/game/join/<int:match_id>', methods=['POST'])
def join_game(match_id):
    """
    Join an existing game
    Expected JSON: {"player_id": 2, "player_name": "bob"}
    """
    try:
        data = request.get_json()
        player_id = data.get('player_id')
        player_name = data.get('player_name')

        if not all([player_id, player_name]):
            return jsonify({"error": "Missing required fields"}), 400

        result = game_service.join_game(match_id, player_id, player_name)

        if result["success"]:
            return jsonify(result)
        else:
            return jsonify({"error": result["error"]}), 500

    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/game/surrender', methods=['POST'])
def surrender_game():
    """
    Surrender the current game
    Expected JSON: {"match_id": 1, "player_id": 1}
    """
    try:
        data = request.get_json()
        match_id = data.get('match_id')
        player_id = data.get('player_id')

        if not all([match_id, player_id]):
            return jsonify({"error": "Missing required fields"}), 400

        result = game_service.surrender_game(match_id, player_id)

        if result["success"]:
            return jsonify(result)
        else:
            return jsonify({"error": result["error"]}), 500

    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/history/<int:user_id>', methods=['GET'])
def get_game_history(user_id):
    """
    Get game history for a user
    Returns list of matches with basic info
    """
    try:
        result = history_service.get_game_history(user_id)

        if result["success"]:
            return jsonify(result)
        else:
            return jsonify({"error": result["error"], "history": []}), 500

    except Exception as e:
        return jsonify({"error": str(e), "history": []}), 500

@app.route('/api/history/replay/<int:match_id>', methods=['GET'])
def get_game_replay(match_id):
    """
    Get detailed replay of a specific match
    Returns all moves with FEN states
    """
    try:
        result = history_service.get_game_replay(match_id)

        if result["success"]:
            return jsonify(result)
        else:
            return jsonify({"error": result["error"], "replay": {}}), 500

    except Exception as e:
        return jsonify({"error": str(e), "replay": {}}), 500

@app.route('/api/stats/<int:user_id>', methods=['GET'])
def get_player_stats(user_id):
    """
    Get player statistics
    """
    try:
        result = history_service.get_player_stats(user_id)

        if result["success"]:
            return jsonify(result)
        else:
            return jsonify({"error": result["error"], "stats": {}}), 500

    except Exception as e:
        return jsonify({"error": str(e), "stats": {}}), 500

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)
