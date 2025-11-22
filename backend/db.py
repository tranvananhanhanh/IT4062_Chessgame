# backend/db.py
import os
import psycopg2


PG_DBNAME = os.getenv("PGDATABASE", "it4062_chessgame")
PG_USER = os.getenv("PGUSER", "postgres")
PG_PASSWORD = os.getenv("PGPASSWORD", "123456")
PG_HOST = os.getenv("PGHOST", "localhost")
PG_PORT = os.getenv("PGPORT", "5432")


def get_db():
    """
    Tạo connection tới PostgreSQL.
    Mỗi lần gọi API thì tạo 1 connection mới, dùng xong nhớ close().
    """
    conn = psycopg2.connect(
        dbname=PG_DBNAME,
        user=PG_USER,
        password=PG_PASSWORD,
        host=PG_HOST,
        port=PG_PORT,
    )
    return conn


def init_db():
    """
    Khởi tạo bảng users nếu chưa tồn tại.
    """
    conn = get_db()
    cur = conn.cursor()

    cur.execute(
        """
        CREATE TABLE IF NOT EXISTS users (
            id SERIAL PRIMARY KEY,
            username VARCHAR(50) NOT NULL UNIQUE,
            email VARCHAR(255) NOT NULL UNIQUE,
            password_hash TEXT NOT NULL,
            created_at TIMESTAMPTZ NOT NULL
        );
        """
    )

    conn.commit()
    conn.close()
