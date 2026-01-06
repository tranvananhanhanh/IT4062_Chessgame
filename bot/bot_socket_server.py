#!/usr/bin/env python3
"""
Chess Bot Socket Server (FINAL VERSION)

ROLE:
- Receive FEN from C server
- Calculate ONE bot move using python-chess
- Return ONLY bot_move (UCI)

C server is the SINGLE SOURCE OF TRUTH for:
- board state
- move execution
- FEN generation
"""

import socket
import sys
import chess
import random

# Import chess.engine at module level to avoid shadowing
try:
    import chess.engine
    STOCKFISH_AVAILABLE = True
except ImportError:
    STOCKFISH_AVAILABLE = False

HOST = "127.0.0.1"
PORT = 5001


def calculate_bot_move(fen: str, difficulty: str = "easy") -> str:
    """
    Input : FEN (current board state)
    Output: bot_move in UCI format (e2e4, g8f6, etc.)
            or "NOMOVE" if no legal moves
    """
    try:
        board = chess.Board(fen)
        legal_moves = list(board.legal_moves)

        if not legal_moves:
            print("[Bot] No legal moves")
            return "NOMOVE"

        # ===== EASY: random / capture-first =====
        if difficulty == "easy":
            captures = [m for m in legal_moves if board.is_capture(m)]
            move = random.choice(captures or legal_moves)

        # ===== HARD: Stockfish =====
        elif difficulty == "hard":
            if STOCKFISH_AVAILABLE:
                try:
                    with chess.engine.SimpleEngine.popen_uci("stockfish") as engine:
                        result = engine.play(
                            board, chess.engine.Limit(time=0.2)
                        )
                        move = result.move
                except Exception as e:
                    print(f"[Bot] Stockfish error: {e}", file=sys.stderr)
                    move = random.choice(legal_moves)
            else:
                print("[Bot] Stockfish not available, using random", file=sys.stderr)
                move = random.choice(legal_moves)

        else:
            move = random.choice(legal_moves)

        bot_move = move.uci()
        print(f"[Bot] FEN: {fen}")
        print(f"[Bot] Bot move: {bot_move}")

        return bot_move

    except Exception as e:
        print(f"[Bot] Error calculating move: {e}", file=sys.stderr)
        return "ERROR"


def handle_client(client_socket, addr):
    """
    Protocol:
        Request : "fen|difficulty\n"
        Response: "bot_move\n"
    """
    print(f"[Bot] Connection from {addr}")

    try:
        data = client_socket.recv(4096).decode("utf-8").strip()
        if not data:
            print("[Bot] Empty request")
            return

        print(f"[Bot] Received: {data}")

        parts = data.split("|")
        fen = parts[0]
        difficulty = parts[1] if len(parts) > 1 else "easy"

        bot_move = calculate_bot_move(fen, difficulty)

        response = f"{bot_move}\n"
        client_socket.sendall(response.encode("utf-8"))
        print(f"[Bot] Sent: {bot_move}")

    except Exception as e:
        print(f"[Bot] Client handling error: {e}", file=sys.stderr)
        try:
            client_socket.sendall(b"ERROR\n")
        except:
            pass
    finally:
        client_socket.close()


def main():
    print("=" * 50)
    print("     Chess Bot Server (FINAL)")
    print("=" * 50)
    print(f"[Bot] Listening on {HOST}:{PORT}")
    print()

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    # For Linux/Unix: also set SO_REUSEPORT to allow port reuse after crash
    try:
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except (AttributeError, OSError):
        pass  # SO_REUSEPORT not available on Windows
    
    # Set socket timeout to allow graceful shutdown
    server_socket.settimeout(1.0)

    try:
        server_socket.bind((HOST, PORT))
        server_socket.listen(5)
        print("✅ Bot server ready\n")

        while True:
            try:
                client_socket, addr = server_socket.accept()
                handle_client(client_socket, addr)
            except socket.timeout:
                continue  # Timeout is normal, just continue
            except KeyboardInterrupt:
                raise  # Re-raise to outer handler
            except Exception as e:
                print(f"[Bot] Accept error: {e}", file=sys.stderr)
                continue

    except KeyboardInterrupt:
        print("\n[Bot] Shutting down...")
    except Exception as e:
        print(f"[Bot] Server error: {e}", file=sys.stderr)
    finally:
        try:
            server_socket.close()
        except:
            pass
        print("[Bot] Server stopped")


if __name__ == "__main__":
    main()
