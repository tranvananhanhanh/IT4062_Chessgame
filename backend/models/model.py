# models.py
from utils.config import db
from datetime import datetime

class User(db.Model):
    __tablename__ = 'users'
    user_id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(50), unique=True, nullable=False)
    password_hash = db.Column(db.String(255), nullable=False)
    state = db.Column(db.String(20), default='offline')
    elo_point = db.Column(db.Integer, default=1000)
    matches_played = db.relationship('MatchPlayer', backref='user', lazy=True)
    won_matches = db.relationship('MatchGame', backref='winner', foreign_keys='MatchGame.winner_id', lazy=True)

class MatchGame(db.Model):
    __tablename__ = 'match_game'
    match_id = db.Column(db.Integer, primary_key=True)
    type = db.Column(db.String(10), default='pvp')
    status = db.Column(db.String(15), default='waiting')
    starttime = db.Column(db.DateTime, default=datetime.utcnow)
    endtime = db.Column(db.DateTime, nullable=True)
    result = db.Column(db.String(15), nullable=True)
    winner_id = db.Column(db.Integer, db.ForeignKey('users.user_id', onupdate='CASCADE', ondelete='SET NULL'), nullable=True)
    players = db.relationship('MatchPlayer', backref='match', lazy=True)
    moves = db.relationship('Move', backref='match', lazy=True)

class MatchPlayer(db.Model):
    __tablename__ = 'match_player'
    match_player_id = db.Column(db.Integer, primary_key=True)
    match_id = db.Column(db.Integer, db.ForeignKey('match_game.match_id', onupdate='CASCADE', ondelete='CASCADE'), nullable=False)
    user_id = db.Column(db.Integer, db.ForeignKey('users.user_id', onupdate='CASCADE', ondelete='SET NULL'), nullable=True)
    color = db.Column(db.String(10), nullable=False)
    score = db.Column(db.Float, default=0)
    is_bot = db.Column(db.Boolean, default=False)
    action_state = db.Column(db.String(20), default='normal')

class Move(db.Model):
    __tablename__ = 'move'
    move_id = db.Column(db.Integer, primary_key=True)
    match_id = db.Column(db.Integer, db.ForeignKey('match_game.match_id', onupdate='CASCADE', ondelete='CASCADE'), nullable=False)
    user_id = db.Column(db.Integer, db.ForeignKey('users.user_id', onupdate='CASCADE', ondelete='SET NULL'), nullable=True)
    notation = db.Column(db.String(20), nullable=False)
    fen_after = db.Column(db.Text, nullable=False)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
