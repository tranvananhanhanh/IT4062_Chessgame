from flask_sqlalchemy import SQLAlchemy
from os import getenv
from sqlalchemy import create_engine, text


class Config:
    """Configuration object for Flask app.config.from_object(Config)."""
    SQLALCHEMY_DATABASE_URI = getenv('DATABASE_URL', 'postgresql://postgres:0000@localhost:5432/chess_game')
    SQLALCHEMY_TRACK_MODIFICATIONS = False


# Global SQLAlchemy instance to be initialized with the Flask app in app.py
# Example in app.py:
#   from utils.config import db, Config
#   db.init_app(app)
#   with app.app_context():
#       db.create_all()

db = SQLAlchemy()


def test_connection():
    """Optional helper to test DB connectivity.
    Call this manually (do not run at import time).
    """
    engine = create_engine(Config.SQLALCHEMY_DATABASE_URI)
    try:
        with engine.connect() as conn:
            conn.execute(text('SELECT 1'))
        print("✅ Kết nối database thành công!")
    except Exception as e:
        print("❌ Kết nối database thất bại:", e)

