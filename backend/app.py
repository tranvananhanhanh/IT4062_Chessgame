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
from models import db, User, MatchGame, MatchPlayer
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

db.init_app(app)

# Initialize services
game_service = get_game_service()
history_service = get_history_service()
auth_service = get_auth_service()     # mới cho register

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

# ==================== MATCHMAKING (KHÔNG DÙNG TOKEN) ====================

def create_match_from_matchmaking(current_user: User, opponent_username: str, my_color: str) -> MatchGame:
    """
    Được gọi khi matchmaking trả về status = 'matched'.

    - Tạo 1 bản ghi match_game (type='pvp', status='playing')
    - Tạo 2 bản ghi match_player (white/black)
    - Đổi state 2 user thành 'in_game'
    """
    # Tìm đối thủ theo username
    opponent = User.query.filter_by(name=opponent_username).first()
    if not opponent:
        raise Exception(f"Opponent user not found: {opponent_username}")

    # Tạo match_game
    match = MatchGame(
        type="pvp",
        status="playing",
    )
    db.session.add(match)
    db.session.flush()  # để có match.match_id

    # Xác định màu mình & đối thủ
    if my_color == "white":
        me_color = "white"
        opp_color = "black"
    else:
        me_color = "black"
        opp_color = "white"

    # Mình
    mp_me = MatchPlayer(
        match_id=match.match_id,
        user_id=current_user.user_id,
        color=me_color,
        is_bot=False,
    )

    # Đối thủ
    mp_opp = MatchPlayer(
        match_id=match.match_id,
        user_id=opponent.user_id,
        color=opp_color,
        is_bot=False,
    )

    db.session.add(mp_me)
    db.session.add(mp_opp)

    # Đổi state 2 thằng thành in_game
    current_user.state = "in_game"
    opponent.state = "in_game"

    db.session.commit()
    return match

@app.post("/matchmaking/join")
def matchmaking_join():
    data = request.get_json() or {}
    username = data.get("username")

    if not username:
        return jsonify({"error": "username is required"}), 400

    user = User.query.filter_by(name=username).first()
    if not user:
        return jsonify({"error": "User not found"}), 404

    # 0) Không cho join nếu user đang ở trong một trận active
    active_match = (
        db.session.query(MatchGame)
        .join(MatchPlayer, MatchGame.match_id == MatchPlayer.match_id)
        .filter(
            MatchPlayer.user_id == user.user_id,
            MatchGame.status.in_(["waiting", "playing", "paused"])
        )
        .first()
    )

    if active_match:
        return jsonify({
            "error": "User is already in an active match",
            "match_id": active_match.match_id,
            "match_status": active_match.status,
        }), 400

    # 1) Gửi sang C server để ghép cặp
    try:
        result = join_matchmaking(user.name, user.elo_point)
    except Exception as e:
        return jsonify({"error": str(e)}), 500

    status = result.get("status")

    # 2) Chỉ vào hàng đợi
    if status == "queued":
        # đảm bảo state = 'online'
        if user.state != "online":
            user.state = "online"
            db.session.commit()
        return jsonify(result)

    # 3) Đã ghép cặp thành công -> tạo match_game + match_player + state = in_game
    if status == "matched":
        opponent_username = result.get("opponent_id")
        my_color = result.get("color")

        try:
            match = create_match_from_matchmaking(user, opponent_username, my_color)
        except Exception as e:
            db.session.rollback()
            return jsonify({"error": f"Failed to create match in DB: {str(e)}"}), 500

        # Gắn thêm match_id trong DB trả về cho frontend
        result["db_match_id"] = match.match_id
        return jsonify(result)

    # 4) Trường hợp không mong đợi
    return jsonify({
        "error": "Unknown matchmaking status",
        "raw": result,
    }), 500

@app.get("/matchmaking/status")
def matchmaking_status():
    username = request.args.get("username")

    if not username:
        return jsonify({"error": "username is required"}), 400

    user = User.query.filter_by(name=username).first()
    if not user:
        return jsonify({"error": "User not found"}), 404

    result = check_match_status(user.name)
    return jsonify(result)


@app.post("/matchmaking/cancel")
def matchmaking_cancel():
    data = request.get_json() or {}
    username = data.get("username")

    if not username:
        return jsonify({"error": "username is required"}), 400

    user = User.query.filter_by(name=username).first()
    if not user:
        return jsonify({"error": "User not found"}), 404

    result = cancel_matchmaking(user.name)
    return jsonify(result)


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

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5002)
