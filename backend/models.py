from flask_sqlalchemy import SQLAlchemy

db = SQLAlchemy()


class User(db.Model):
    __tablename__ = "users"

    user_id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(50), unique=True, nullable=False)
    email = db.Column(db.String(100), unique=True)
    password_hash = db.Column(db.String(255), nullable=False)
    state = db.Column(db.String(20), default="offline")
    elo_point = db.Column(db.Integer, default=1000)

    def to_dict(self):
        return {
            "user_id": self.user_id,
            "username": self.name,
            "email": self.email,
            "elo_point": self.elo_point,
            "state": self.state,
        }
class MatchGame(db.Model):
    __tablename__ = "match_game"

    match_id = db.Column(db.Integer, primary_key=True)
    type = db.Column(db.String(10), default="pvp")  # 'pvp' hoặc 'bot'
    status = db.Column(db.String(15), default="waiting")  # 'waiting', 'playing', ...
    starttime = db.Column(db.DateTime, server_default=db.func.now())
    endtime = db.Column(db.DateTime, nullable=True)
    result = db.Column(db.String(15), nullable=True)  # 'white_win', 'black_win', ...
    winner_id = db.Column(db.Integer, db.ForeignKey("users.user_id"), nullable=True)

    winner = db.relationship("User", foreign_keys=[winner_id])

    def to_dict(self):
        return {
            "match_id": self.match_id,
            "type": self.type,
            "status": self.status,
            "starttime": self.starttime.isoformat() if self.starttime else None,
            "endtime": self.endtime.isoformat() if self.endtime else None,
            "result": self.result,
            "winner_id": self.winner_id,
        }


class MatchPlayer(db.Model):
    __tablename__ = "match_player"

    match_player_id = db.Column(db.Integer, primary_key=True)
    match_id = db.Column(db.Integer, db.ForeignKey("match_game.match_id"), nullable=False)
    user_id = db.Column(db.Integer, db.ForeignKey("users.user_id"), nullable=True)
    color = db.Column(db.String(10), nullable=False)  # 'white' hoặc 'black'
    score = db.Column(db.Float, default=0.0)
    is_bot = db.Column(db.Boolean, default=False)
    action_state = db.Column(db.String(20), default="normal")  # 'normal', 'surrender', ...

    match = db.relationship("MatchGame", backref="players")
    user = db.relationship("User", backref="matches")

    def to_dict(self):
        return {
            "match_player_id": self.match_player_id,
            "match_id": self.match_id,
            "user_id": self.user_id,
            "color": self.color,
            "score": self.score,
            "is_bot": self.is_bot,
            "action_state": self.action_state,
        }
