from flask import Flask, request, jsonify
from flask_cors import CORS
import uuid
import json
from services.socket_bridge import send_to_c_server
from utils.config import Config, db
from models.model import User, MatchGame, MatchPlayer, Move
from utils.config import test_connection

app = Flask(__name__)
app.config.from_object(Config)
CORS(app)

db.init_app(app)
test_connection()

# Helper to create tables (call once at startup or via a separate script)
with app.app_context():
    db.create_all()

@app.route('/api/game/move', methods=['POST'])
def make_move():
    data = request.get_json() or {}
    game_id = data.get('game_id')
    player_move = data.get('move')
    user_id = data.get('user_id')  # optional but recommended

    if not game_id or not player_move:
        return jsonify({"message": "Thiếu game_id hoặc nước đi"}), 400

    # Load match from DB
    match = MatchGame.query.get(game_id)
    if not match:
        return jsonify({"message": "Không tìm thấy trận đấu"}), 404

    # Determine current FEN from last move or default starting position
    last_move = (
        Move.query.filter_by(match_id=game_id)
        .order_by(Move.created_at.desc())
        .first()
    )
    current_fen = last_move.fen_after if last_move else 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'

    command = f"MOVE|{game_id}|{current_fen}|{player_move}"
    print(f"[Flask->C] Sending command: {command}")

    try:
        response_data = send_to_c_server(command)
        print(f"[C->Flask] Received response: {response_data}")

        parts = response_data.split('|')
        status_code = parts[0]

        # Giờ phải là 5 phần
        if status_code == 'OK' and len(parts) >= 5:
            fen_after_player = parts[1]  # FEN sau nước người
            ai_move = parts[2]           # UCI bot
            fen_after_bot = parts[3]     # FEN sau nước bot
            status_msg = parts[4]        # IN_GAME / CHECKMATE ...

            # Lưu move của người
            move_player = Move(
                match_id=game_id,
                user_id=user_id,
                notation=player_move,
                fen_after=fen_after_player,
            )
            db.session.add(move_player)

            # Lưu move của bot (nếu có)
            if ai_move and ai_move != 'NONE':
                move_ai = Move(
                    match_id=game_id,
                    user_id=None,      # hoặc id user bot tùy bạn
                    notation=ai_move,
                    fen_after=fen_after_bot,
                )
                db.session.add(move_ai)

            db.session.commit()

            return jsonify({
                # tuỳ bạn muốn trả gì cho FE:
                "ai_move": ai_move,
                "fen_after_player": fen_after_player,
                "fen_after_bot": fen_after_bot,
                "status": status_msg,
            }), 200
        else:
            error_message = parts[1] if len(parts) > 1 else "Lỗi không xác định từ C Server"
            return jsonify({"message": f"Lỗi Game Server: {error_message}"}), 500

    except Exception as e:
        db.session.rollback()
        print(f"Socket Bridge Error: {e}")
        return jsonify({"message": f"Lỗi kết nối Game Server (C): {str(e)}"}), 503


@app.route('/api/mode/bot', methods=['POST'])
def start_bot_mode():
    """Create a new bot match in DB and ask C server for initial FEN/state."""
    data = request.get_json() or {}
    user_id = data.get('user_id')  # optional: who is playing vs bot

    # Create match in DB
    match = MatchGame(type='bot', status='playing')
    db.session.add(match)
    db.session.flush()  # get match_id

    # Create players: human (white) and bot (black)
    if user_id:
        db.session.add(MatchPlayer(match_id=match.match_id, user_id=user_id, color='white', is_bot=False))
    db.session.add(MatchPlayer(match_id=match.match_id, user_id=None, color='black', is_bot=True))

    command = "MODE BOT"
    print(f"[Flask->C] Sending MODE BOT command (public)")
    try:
        # call c server 
        response_data = send_to_c_server(command)
        print(f"[C->Flask] Received MODE response: {response_data}")

        parts = response_data.split('|')
        if parts[0] != 'OK':
            db.session.rollback()
            err = parts[1] if len(parts) > 1 else 'Unknown error from C server'
            return jsonify({"message": f"Lỗi Game Server: {err}"}), 500

        new_fen = parts[1] if len(parts) > 1 else None
        ai_move = parts[2] if len(parts) > 2 and parts[2] != 'NONE' else None
        status_msg = parts[3] if len(parts) > 3 else 'OK'

        # Persist initial position as a Move if you want history
        if new_fen:
            db.session.add(Move(match_id=match.match_id, user_id=None, notation='START', fen_after=new_fen))

        db.session.commit()

        return jsonify({
            "game_id": match.match_id,
            "fen": new_fen,
            "ai_move": ai_move,
            "status": status_msg
        }), 200

    except Exception as e:
        db.session.rollback()
        print(f"Socket Bridge Error: {e}")
        return jsonify({"message": f"Lỗi kết nối Game Server (C): {str(e)}"}), 503


@app.route('/api/game/control', methods=['POST'])
def game_control():
    data = request.get_json() or {}
    game_id = data.get('game_id')
    action = data.get('action')
    user_id = data.get('user_id', 1)

    if not game_id or not action:
        return jsonify({"message": "Thiếu game_id hoặc hành động (action)"}), 400

    action = action.upper()

    command = f"CONTROL|{game_id}|{user_id}|{action}"
    print(f"[Flask->C] Sending control command: {command}")

    try:
        response_data = send_to_c_server(command)
        print(f"[C->Flask] Received control response: {response_data}")

        parts = response_data.split('|')
        status_code = parts[0]

        if status_code == 'OK' and len(parts) > 1:
            status_msg = parts[1]

            # Update match status/result based on action
            match = MatchGame.query.get(game_id)
            if match:
                if action == 'SURRENDER':
                    # Người chơi đầu hàng -> ván kết thúc, kết quả surrender
                    match.status = 'finished'
                    match.result = 'surrender'
                    # TODO: sau này có thể set winner_id theo màu đối thủ
                elif action == 'DRAW_ACCEPT':
                    # Chỉ khi đối thủ chấp nhận hòa mới kết thúc ván
                    match.status = 'finished'
                    match.result = 'draw'
                elif action in ('PAUSE', 'RESUME'):
                    # Ván vẫn đang chơi, trạng thái logic PAUSED được giữ ở C server
                    match.status = 'playing'
                # Các action khác như DRAW (đề nghị), DRAW_DECLINE, REMATCH...
                # không thay đổi trạng thái match_game trong DB ở thời điểm này.
                db.session.commit()

            return jsonify({
                "message": f"Thực hiện hành động {action} thành công.",
                "status_c_server": status_msg
            }), 200
        else:
            error_message = parts[1] if len(parts) > 1 else "Lỗi không xác định từ C Server"
            return jsonify({"message": f"Lỗi thực hiện hành động: {error_message}"}), 500

    except Exception as e:
        db.session.rollback()
        print(f"Socket Bridge Error: {e}")
        return jsonify({"message": f"Lỗi kết nối Game Server (C) khi điều khiển: {str(e)}"}), 503


if __name__ == '__main__':
    app.run(debug=True, port=5000)
