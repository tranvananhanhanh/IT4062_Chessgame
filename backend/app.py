from flask import Flask, request, jsonify
from flask_cors import CORS
from services.game_service import get_game_service
from services.history_service import get_history_service
from services.auth_service import get_auth_service
from services.matchmaking_service import (
    join_matchmaking,
    check_match_status,
    cancel_matchmaking,
)
from services.login_service import get_login_service
from services.friend_service import get_friend_service
import os

app = Flask(__name__)
CORS(app)  # Enable CORS for React frontend

# ==================== DATABASE CONFIG ====================
DATABASE_URL = os.getenv(
    "DATABASE_URL",
    "postgresql://postgres:123456@localhost:5432/chess_db"  # <-- tùy chỉnh
)

app.config["SQLALCHEMY_DATABASE_URI"] = DATABASE_URL
app.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = False

# Initialize services
game_service = get_game_service()
history_service = get_history_service()
auth_service = get_auth_service()     # mới cho register
login_service = get_login_service()
friend_service = get_friend_service()

@app.route('/api/auth/register', methods=['POST'])
def register():
    """
    Register a new user
    Expected JSON: {"username": "alice", "password": "123456"}
    """
    try:
        data = request.get_json() or {}
        username = data.get("username")
        email = data.get("email")
        password = data.get("password")

        if not username or not password:
            return jsonify({
                "success": False,
                "error": "username and password are required"
            }), 400

        result = auth_service.register(username, password, email)

        status_code = 201 if result.get("success") else 400
        return jsonify(result), status_code

    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/auth/login', methods=['POST'])
def login():
    """
    Login API
    Expected JSON: {"username": "alice", "password": "123456"}
    Returns: {success, user_id, username, elo, message, error}
    """
    try:
        data = request.get_json() or {}
        username = data.get("username")
        password = data.get("password")
        if not username or not password:
            return jsonify({"success": False, "error": "username and password are required"}), 400
        result = login_service.login(username, password)
        status_code = 200 if result.get("success") else 400
        return jsonify(result), status_code
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

# ==================== MATCHMAKING (KHÔNG DÙNG TOKEN) ====================

@app.post("/matchmaking/join")
def matchmaking_join():
    data = request.get_json() or {}
    username = data.get("username")
    elo = data.get("elo")
    time_mode = data.get("time_mode", "BLITZ")

    if not username:
        return jsonify({"error": "username is required"}), 400

    # Nếu chưa có elo, tự lấy từ C server
    if elo is None:
        try:
            from services.socket_bridge import send_request
            resp = send_request(f"GET_ELO|{username}")
            if resp and resp.strip().isdigit():
                elo = int(resp.strip())
            else:
                elo = 1000  # mặc định nếu không lấy được
        except Exception:
            elo = 1000

    try:
        result = join_matchmaking(username, elo, time_mode)
    except Exception as e:
        return jsonify({"error": str(e)}), 500

    status = result.get("status")
    if status == "queued":
        return jsonify(result)
    elif status == "matched":
        return jsonify(result)
    else:
        return jsonify({"error": "Unknown matchmaking status", "raw": result}), 500

@app.get("/matchmaking/status")
def matchmaking_status():
    username = request.args.get("username")

    if not username:
        return jsonify({"error": "username is required"}), 400

    # Delegate matchmaking status to C server
    result = check_match_status(username)
    return jsonify(result)

@app.post("/matchmaking/cancel")
def matchmaking_cancel():
    data = request.get_json() or {}
    username = data.get("username")

    if not username:
        return jsonify({"error": "username is required"}), 400

    # Delegate matchmaking cancel to C server
    result = cancel_matchmaking(username)
    return jsonify(result)

@app.route('/api/health', methods=['GET'])
def health_check():
    """Health check endpoint"""
    return jsonify({"status": "ok", "message": "Flask backend is running"})

@app.route('/api/game/start', methods=['POST'])
def start_game():
    data = request.get_json()
    player1_id = data.get('player1_id')
    player1_name = data.get('player1_name')
    player2_id = data.get('player2_id')
    player2_name = data.get('player2_name')
    if not all([player1_id, player1_name, player2_id, player2_name]):
        return jsonify({"error": "Missing required fields"}), 400
    # Delegate to C server
    result = game_service.start_game(player1_id, player1_name, player2_id, player2_name)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"error": result.get("error", "Unknown error")}), 500

@app.route('/api/game/move', methods=['POST'])
def make_move():
    data = request.get_json()
    match_id = data.get('match_id')
    player_id = data.get('player_id')
    from_square = data.get('from_square')
    to_square = data.get('to_square')
    if not all([match_id, player_id, from_square, to_square]):
        return jsonify({"error": "Missing required fields"}), 400
    # Delegate to C server
    result = game_service.make_move(match_id, player_id, from_square, to_square)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"error": result.get("error", "Invalid move")}), 400

@app.route('/api/game/join/<int:match_id>', methods=['POST'])
def join_game(match_id):
    data = request.get_json()
    player_id = data.get('player_id')
    player_name = data.get('player_name')
    if not all([player_id, player_name]):
        return jsonify({"error": "Missing required fields"}), 400
    # Delegate to C server
    result = game_service.join_game(match_id, player_id, player_name)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"error": result.get("error", "Failed to join game")}), 500

@app.route('/api/game/status/<int:match_id>', methods=['GET'])
def get_match_status(match_id):
    result = game_service.get_match_status(match_id)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"error": result.get("error", "Match not found")}), 404

@app.route('/api/game/surrender', methods=['POST'])
def surrender_game():
    data = request.get_json()
    match_id = data.get('match_id')
    player_id = data.get('player_id')
    if not all([match_id, player_id]):
        return jsonify({"error": "Missing required fields"}), 400
    # Delegate to C server
    result = game_service.surrender_game(match_id, player_id)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"error": result.get("error", "Failed to surrender")}), 500

@app.route('/api/game/control', methods=['POST'])
def game_control():
    data = request.get_json()
    match_id = data.get('match_id')
    player_id = data.get('player_id')
    action = data.get('action', '').upper()
    valid_actions = ['PAUSE', 'RESUME', 'DRAW', 'DRAW_ACCEPT', 'DRAW_DECLINE', 'REMATCH', 'REMATCH_ACCEPT', 'REMATCH_DECLINE']
    if not all([match_id, player_id, action]) or action not in valid_actions:
        return jsonify({"error": "Missing or invalid fields"}), 400
    # Delegate to C server
    result = game_service.game_control(match_id, player_id, action)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"error": result.get("error", "Failed to control game")}), 500

@app.route('/api/mode/bot', methods=['POST'])
def start_bot_mode():
    data = request.get_json()
    user_id = data.get('user_id')
    if not user_id:
        return jsonify({"error": "Missing user_id"}), 400
    # Delegate to C server
    result = game_service.start_bot_game(user_id)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"error": result.get("error", "Failed to start bot game")}), 500

@app.route('/api/game/bot/move', methods=['POST'])
def make_bot_move():
    data = request.get_json()
    match_id = data.get('match_id')
    move = data.get('move')
    if not all([match_id, move]):
        return jsonify({"error": "Missing required fields"}), 400
    # Delegate to C server
    result = game_service.make_bot_move(match_id, move)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"error": result.get("error", "Failed bot move")}), 500

@app.route('/api/history/<int:user_id>', methods=['GET'])
def get_game_history(user_id):
    # Delegate to C server
    result = history_service.get_game_history(user_id)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"error": result.get("error", "Failed to get history"), "history": []}), 500

@app.route('/api/history/replay/<int:match_id>', methods=['GET'])
def get_game_replay(match_id):
    # Delegate to C server
    result = history_service.get_game_replay(match_id)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"error": result.get("error", "Failed to get replay"), "replay": {}}), 500

@app.route('/api/stats/<int:user_id>', methods=['GET'])
def get_player_stats(user_id):
    # Delegate to C server
    result = history_service.get_player_stats(user_id)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"error": result.get("error", "Failed to get stats"), "stats": {}}), 500

# ==================== FRIENDSHIP API ====================

@app.route('/api/friend/request', methods=['POST'])
def send_friend_request():
    data = request.get_json()
    user_id = data.get('user_id')
    friend_id = data.get('friend_id')
    if not user_id or not friend_id or user_id == friend_id:
        return jsonify({"success": False, "error": "Invalid user_id or friend_id"}), 400
    # Delegate to FriendService
    result = friend_service.send_friend_request(user_id, friend_id)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"success": False, "error": result.get("error", "Failed to send friend request")}), 400

@app.route('/api/friend/accept', methods=['POST'])
def accept_friend_request():
    data = request.get_json()
    user_id = data.get('user_id')
    friend_id = data.get('friend_id')
    if not user_id or not friend_id:
        return jsonify({"success": False, "error": "Invalid user_id or friend_id"}), 400
    result = friend_service.accept_friend_request(user_id, friend_id)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"success": False, "error": result.get("error", "Failed to accept friend request")}), 400

@app.route('/api/friend/decline', methods=['POST'])
def decline_friend_request():
    data = request.get_json()
    user_id = data.get('user_id')
    friend_id = data.get('friend_id')
    if not user_id or not friend_id:
        return jsonify({"success": False, "error": "Invalid user_id or friend_id"}), 400
    result = friend_service.decline_friend_request(user_id, friend_id)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"success": False, "error": result.get("error", "Failed to decline friend request")}), 400

@app.route('/api/friend/list/<int:user_id>', methods=['GET'])
def list_friends(user_id):
    result = friend_service.list_friends(user_id)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"success": False, "error": result.get("error", "Failed to get friend list"), "friends": []}), 400

@app.route('/api/friend/requests/<int:user_id>', methods=['GET'])
def list_friend_requests(user_id):
    result = friend_service.list_friend_requests(user_id)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"success": False, "error": result.get("error", "Failed to get friend requests"), "requests": []}), 400

@app.route('/api/game/create', methods=['POST'])
def create_game():
    data = request.get_json()
    user1_id = data.get('user1_id')
    username1 = data.get('username1')
    user2_id = data.get('user2_id')
    username2 = data.get('username2')
    if not all([user1_id, username1, user2_id, username2]):
        return jsonify({"error": "Missing required fields"}), 400
    # Delegate to C server
    result = game_service.create_game(user1_id, username1, user2_id, username2)
    if result.get("success"):
        return jsonify(result)
    else:
        return jsonify({"error": result.get("error", "Failed to create game")}), 500

@app.route('/api/elo/leaderboard', methods=['GET'])
def get_leaderboard():
    """
    Get ELO leaderboard (top players)
    Optional query param: limit (default 20)
    """
    try:
        from services.elo_service import get_elo_service
        elo_service = get_elo_service()
        limit = int(request.args.get("limit", 20))
        result = elo_service.get_leaderboard(limit)
        return jsonify(result)
    except Exception as e:
        return jsonify({"success": False, "error": str(e), "leaderboard": []}), 500

@app.route('/api/elo/history/<int:user_id>', methods=['GET'])
def get_elo_history(user_id):
    """
    Get ELO history for a user
    """
    try:
        from services.elo_service import get_elo_service
        elo_service = get_elo_service()
        result = elo_service.get_elo_history(user_id)
        return jsonify(result)
    except Exception as e:
        return jsonify({"success": False, "error": str(e), "elo_history": []}), 500

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5002)
