from flask import Flask, request, jsonify
from flask_cors import CORS
from services.game_service import get_game_service
from services.history_service import get_history_service

app = Flask(__name__)
CORS(app)  # Enable CORS for React frontend

# Initialize services
game_service = get_game_service()
history_service = get_history_service()

@app.route('/health', methods=['GET'])
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

@app.route('/api/game/status/<int:match_id>', methods=['GET'])
def get_match_status(match_id):
    """
    Get current status of a match (for polling)
    Returns: {"match_id": int, "status": str, "white_online": bool, "black_online": bool, "current_fen": str}
    """
    try:
        result = game_service.get_match_status(match_id)
        
        if result["success"]:
            return jsonify(result)
        else:
            return jsonify({"error": result["error"]}), 404

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

@app.route('/api/game/control', methods=['POST'])
def game_control():
    """
    Game control actions: pause, resume, draw request/accept/decline, rematch
    Expected JSON: {"match_id": 1, "player_id": 1, "action": "REMATCH"}
    Actions: PAUSE, RESUME, DRAW, DRAW_ACCEPT, DRAW_DECLINE, REMATCH, REMATCH_ACCEPT, REMATCH_DECLINE
    """
    try:
        data = request.get_json()
        match_id = data.get('match_id')
        player_id = data.get('player_id')
        action = data.get('action', '').upper()

        if not all([match_id, player_id, action]):
            return jsonify({"error": "Missing required fields"}), 400

        # ✅ Add rematch actions to valid actions
        valid_actions = ['PAUSE', 'RESUME', 'DRAW', 'DRAW_ACCEPT', 'DRAW_DECLINE', 
                        'REMATCH', 'REMATCH_ACCEPT', 'REMATCH_DECLINE']
        if action not in valid_actions:
            return jsonify({"error": f"Invalid action. Must be one of: {', '.join(valid_actions)}"}), 400

        result = game_service.game_control(match_id, player_id, action)

        if result["success"]:
            return jsonify(result)
        else:
            return jsonify({"error": result["error"]}), 500

    except Exception as e:
        return jsonify({"error": str(e)}), 500

# ==================== BOT MODE ENDPOINTS ====================

@app.route('/api/mode/bot', methods=['POST'])
def start_bot_mode():
    """
    Initialize a bot game
    Expected JSON: {"user_id": 1}
    """
    try:
        data = request.get_json()
        user_id = data.get('user_id')

        if not user_id:
            return jsonify({"error": "Missing user_id"}), 400

        result = game_service.start_bot_game(user_id)

        if result["success"]:
            return jsonify(result)
        else:
            return jsonify({"error": result["error"]}), 500

    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/game/bot/move', methods=['POST'])
def make_bot_move():
    """
    Make a move in bot game
    Expected JSON: {"match_id": 1, "move": "e2e4"}
    Returns player move result + bot response
    """
    try:
        data = request.get_json()
        match_id = data.get('match_id')
        move = data.get('move')

        if not all([match_id, move]):
            return jsonify({"error": "Missing required fields"}), 400

        result = game_service.make_bot_move(match_id, move)

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

# ==================== FRIENDSHIP API ====================

@app.route('/api/friend/request', methods=['POST'])
def send_friend_request():
    """
    Send a friend request
    Body: { "user_id": 1, "friend_id": 2 }
    """
    try:
        data = request.get_json()
        user_id = data.get('user_id')
        friend_id = data.get('friend_id')
        if not user_id or not friend_id or user_id == friend_id:
            return jsonify({"success": False, "error": "Invalid user_id or friend_id"}), 400
        # Gửi command tới C server
        command = f"FRIEND_REQUEST|{user_id}|{friend_id}"
        response = game_service.c_bridge.send_command(command)
        if response and response.startswith("FRIEND_REQUESTED"):
            return jsonify({"success": True, "message": "Friend request sent"})
        else:
            return jsonify({"success": False, "error": response or "Failed to send friend request"}), 400
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/friend/accept', methods=['POST'])
def accept_friend_request():
    """
    Accept a friend request
    Body: { "user_id": 2, "friend_id": 1 }
    """
    try:
        data = request.get_json()
        user_id = data.get('user_id')
        friend_id = data.get('friend_id')
        if not user_id or not friend_id:
            return jsonify({"success": False, "error": "Invalid user_id or friend_id"}), 400
        command = f"FRIEND_ACCEPT|{user_id}|{friend_id}"
        response = game_service.c_bridge.send_command(command)
        if response and response.startswith("FRIEND_ACCEPTED"):
            return jsonify({"success": True, "message": "Friend request accepted"})
        else:
            return jsonify({"success": False, "error": response or "Failed to accept friend request"}), 400
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/friend/decline', methods=['POST'])
def decline_friend_request():
    """
    Decline a friend request
    Body: { "user_id": 2, "friend_id": 1 }
    """
    try:
        data = request.get_json()
        user_id = data.get('user_id')
        friend_id = data.get('friend_id')
        if not user_id or not friend_id:
            return jsonify({"success": False, "error": "Invalid user_id or friend_id"}), 400
        command = f"FRIEND_DECLINE|{user_id}|{friend_id}"
        response = game_service.c_bridge.send_command(command)
        if response and response.startswith("FRIEND_DECLINED"):
            return jsonify({"success": True, "message": "Friend request declined"})
        else:
            return jsonify({"success": False, "error": response or "Failed to decline friend request"}), 400
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/friend/list/<int:user_id>', methods=['GET'])
def list_friends(user_id):
    """
    Get accepted friends list
    """
    try:
        command = f"FRIEND_LIST|{user_id}"
        response = game_service.c_bridge.send_command(command)
        if response and response.startswith("FRIEND_LIST|"):
            ids = response.strip().split('|')[1]
            friend_ids = ids.split(',') if ids else []
            return jsonify({"success": True, "friends": friend_ids})
        else:
            return jsonify({"success": False, "error": response or "Failed to get friend list", "friends": []}), 400
    except Exception as e:
        return jsonify({"success": False, "error": str(e), "friends": []}), 500

@app.route('/api/friend/requests/<int:user_id>', methods=['GET'])
def list_friend_requests(user_id):
    """
    Get pending friend requests
    """
    try:
        command = f"FRIEND_REQUESTS|{user_id}"
        response = game_service.c_bridge.send_command(command)
        if response and response.startswith("FRIEND_REQUESTS|"):
            ids = response.strip().split('|')[1]
            request_ids = ids.split(',') if ids else []
            return jsonify({"success": True, "requests": request_ids})
        else:
            return jsonify({"success": False, "error": response or "Failed to get friend requests", "requests": []}), 400
    except Exception as e:
        return jsonify({"success": False, "error": str(e), "requests": []}), 500

@app.route('/api/game/create', methods=['POST'])
def create_game():
    """
    Create a new PvP game
    Expected JSON: {"user1_id": 1, "username1": "alice", "user2_id": 2, "username2": "bob"}
    """
    try:
        data = request.get_json()
        user1_id = data.get('user1_id')
        username1 = data.get('username1')
        user2_id = data.get('user2_id')
        username2 = data.get('username2')

        if not all([user1_id, username1, user2_id, username2]):
            return jsonify({"error": "Missing required fields"}), 400

        print(f"[API] Creating game: {username1} (ID:{user1_id}) vs {username2} (ID:{user2_id})")
        
        result = game_service.create_game(user1_id, username1, user2_id, username2)

        if result.get("success"):
            print(f"[API] Game created successfully: match_id={result.get('match_id')}")
            return jsonify(result)
        else:
            error_msg = result.get("error", "Unknown error")
            print(f"[API] Failed to create game: {error_msg}")
            return jsonify({"error": error_msg}), 500

    except Exception as e:
        print(f"[API] Exception in create_game: {str(e)}")
        import traceback
        traceback.print_exc()
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5002)
