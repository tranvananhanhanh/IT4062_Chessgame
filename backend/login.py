import os
import sqlite3
from contextlib import closing

from flask import Blueprint, jsonify, request
from werkzeug.security import check_password_hash

login_bp = Blueprint('login', __name__)


def get_database_path() -> str:
  return os.getenv(
      'DATABASE_PATH',
      os.path.join(os.path.dirname(__file__), 'chess.db'),
  )


def get_connection() -> sqlite3.Connection:
  conn = sqlite3.connect(get_database_path())
  conn.row_factory = sqlite3.Row
  return conn


def verify_password(stored_password: str, plain_password: str) -> bool:
  if stored_password.startswith(('pbkdf2:', 'scrypt:', 'sha256$')):
    return check_password_hash(stored_password, plain_password)
  return stored_password == plain_password


@login_bp.post('/api/login')
def login():
  payload = request.get_json(silent=True) or {}
  username = (payload.get('username') or '').strip()
  password = (payload.get('password') or '').strip()

  if not username or not password:
    return (
        jsonify({'success': False, 'message': 'Tên đăng nhập và mật khẩu là bắt buộc.'}),
        400,
    )

  with closing(get_connection()) as conn:
    cursor = conn.execute(
        'SELECT user_id, name, password_hash FROM users WHERE name = ?', (username,)
    )
    row = cursor.fetchone()
    if row is None or not verify_password(row['password_hash'], password):
      return (
          jsonify({'success': False, 'message': 'Sai tài khoản hoặc mật khẩu.'}),
          401,
      )

    conn.execute('UPDATE users SET state = ? WHERE user_id = ?', ('online', row['user_id']))
    conn.commit()

  return jsonify(
      {
          'success': True,
          'user': {
              'id': row['user_id'],
              'name': row['name'],
          },
      }
  )
