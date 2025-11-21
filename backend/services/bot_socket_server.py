#!/usr/bin/env python3
import socket
import os
import chess
import chess.engine  # or some simple move generator

HOST = os.getenv("BOT_HOST", "127.0.0.1")
PORT = int(os.getenv("BOT_PORT", "5001"))

def handle_client(conn):
    data = b""
    while not data.endswith(b"\n"):
        chunk = conn.recv(4096)
        if not chunk:
            return
        data += chunk

    fen = data.decode("utf-8").strip()
    board = chess.Board(fen)

    # Here you need to know what the human just played.
    # For now you can let python-chess treat this as "state already after human move"
    # and only choose a bot move, so:
    fen_after_player = board.fen()

    # Choose a bot move (random legal move for now)
    legal_moves = list(board.legal_moves)
    if not legal_moves:
        bot_move_uci = "NONE"
        fen_after_bot = board.fen()
    else:
        bot_move = legal_moves[0]
        bot_move_uci = bot_move.uci()
        board.push(bot_move)
        fen_after_bot = board.fen()

    response = f"{fen_after_player}|{bot_move_uci}|{fen_after_bot}\n"
    conn.sendall(response.encode("utf-8"))

def main():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen()
        print(f"[BOT] Listening on {HOST}:{PORT}")
        while True:
            conn, addr = s.accept()
            with conn:
                print(f"[BOT] Connection from {addr}")
                handle_client(conn)

if __name__ == "__main__":
    main()
