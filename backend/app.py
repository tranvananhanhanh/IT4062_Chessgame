import os
import psycopg2
import psycopg2.extras
from contextlib import closing

from flask import Flask, jsonify, request
from flask_cors import CORS
from werkzeug.security import check_password_hash, generate_password_hash
from datetime import datetime, timezone
import re

app = Flask(__name__)
CORS(app, resources={r"/api/*": {"origins": "*"}})

# ===========================
#  PostgreSQL CONFIG
# ===========================

PG_DBNAME = os.getenv("PGDATABASE", "it4062_chessgame")
PG_USER = os.getenv("PGUSER", "postgres")
PG_PASSWORD = os.getenv("PGPASSWORD", "123456")
PG_HOST = os.getenv("PGHOST", "localhost")
PG_PORT = os.getenv("PGPORT", "5432")


def get_connection():
    conn = psycopg2.connect(
        dbname=PG_DBNAME,
        user=PG_USER,
        password=PG_PASSWORD,
        host=PG_HOST,
        port=PG_PORT,
    )
    return conn


# ===========================
#  REGISTER API
# ===========================

EMAIL_REGEX = re.compile(r"^[^@\s]+@[^@\s]+\.[^@\s]+$")

@app.post('/api/register')
def register():
    payload = request.get_json(silent=True) or {}

    username = (payload.get("username") or "").strip()
    email = (payload.get("email") or "").strip().lower()
    password = (payload.get("password") or "")

    if not username or not email or not password:
        return jsonify({'success': False, 'message': 'Thiếu username, email hoặc password.'}), 400

    if not EMAIL_REGEX.match(email):
        return jsonify({'success': False, 'message': 'Email không hợp lệ.'}), 400

    if len(password) < 6:
        return jsonify({'success': False, 'message': 'Mật khẩu phải >= 6 ký tự.'}), 400

    with closing(get_connection()) as conn:
        cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

        # Check username hoặc email trùng
        cur.execute(
            "SELECT user_id, name, email FROM users WHERE name = %s OR email = %s",
            (username, email),
        )
        existing = cur.fetchone()

        if existing:
            if existing["name"] == username:
                return jsonify({'success': False, 'message': 'Tên đăng nhập đã tồn tại.'}), 409
            return jsonify({'success': False, 'message': 'Email đã được sử dụng.'}), 409

        # Insert user
        password_hash = generate_password_hash(password)
        created_at = datetime.now(timezone.utc)

        cur.execute(
            """
            INSERT INTO users (name, email, password_hash, state, created_at)
            VALUES (%s, %s, %s, %s, %s)
            RETURNING user_id;
            """,
            (username, email, password_hash, "offline", created_at),
        )

        new_id = cur.fetchone()["user_id"]
        conn.commit()

    return jsonify({
        'success': True,
        'message': 'Đăng ký thành công.',
        'user': {
            'id': new_id,
            'name': username,
            'email': email,
        }
    }), 201


# ===========================
#  LOGIN API (giữ nguyên logic nhóm bạn)
# ===========================

def verify_password(stored_password: str, plain_password: str) -> bool:
    """Try to verify against hashed password, fall back to plain text."""
    if stored_password.startswith(('pbkdf2:', 'scrypt:', 'sha256$')):
        return check_password_hash(stored_password, plain_password)
    return stored_password == plain_password


@app.post('/api/login')
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
        cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

        cur.execute(
            'SELECT user_id, name, password_hash FROM users WHERE name = %s',
            (username,)
        )
        row = cur.fetchone()

        if row is None or not verify_password(row['password_hash'], password):
            return (
                jsonify({'success': False, 'message': 'Sai tài khoản hoặc mật khẩu.'}),
                401,
            )

        # Update trạng thái online
        cur.execute(
            "UPDATE users SET state = %s WHERE user_id = %s",
            ('online', row['user_id'])
        )
        conn.commit()

    return jsonify({
        'success': True,
        'user': {
            'id': row['user_id'],
            'name': row['name'],
        }
    })


# ===========================
#  SERVER MAIN
# ===========================

if __name__ == '__main__':
    app.run(debug=True, port=5001)
