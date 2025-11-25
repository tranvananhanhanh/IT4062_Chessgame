#!/usr/bin/env python3
"""
Chess Bot Socket Server
Listens on port 5001 for C server connections
Receives FEN and player move, calculates bot move using python-chess
"""

import socket
import sys
import chess
import random

HOST = '127.0.0.1'
PORT = 5001

def calculate_bot_move(fen: str, player_move: str) -> tuple:
    """
    Calculate bot move given current FEN and player's last move
    Returns: (fen_after_player, bot_move, fen_after_bot)
    """
    try:
        # Parse FEN and apply player move
        board = chess.Board(fen)
        
        # Parse player move (e.g., "e2e4")
        if len(player_move) >= 4:
            from_square = chess.parse_square(player_move[0:2])
            to_square = chess.parse_square(player_move[2:4])
            
            # Check for promotion
            promotion = None
            if len(player_move) >= 5:
                promotion_char = player_move[4].lower()
                promotion_map = {'q': chess.QUEEN, 'r': chess.ROOK, 
                                'b': chess.BISHOP, 'n': chess.KNIGHT}
                promotion = promotion_map.get(promotion_char)
            
            move = chess.Move(from_square, to_square, promotion=promotion)
            
            # Apply player move
            if move in board.legal_moves:
                board.push(move)
                fen_after_player = board.fen()
            else:
                print(f"[Bot] Invalid player move: {player_move}", file=sys.stderr)
                return None, None, None
        else:
            # No player move (initial position)
            fen_after_player = fen
        
        # Calculate bot move
        legal_moves = list(board.legal_moves)
        if not legal_moves:
            print("[Bot] No legal moves available", file=sys.stderr)
            return fen_after_player, "NOMOVE", fen_after_player
        
        # Simple strategy: prioritize captures, then random
        capturing_moves = [m for m in legal_moves if board.is_capture(m)]
        
        if capturing_moves:
            bot_move = random.choice(capturing_moves)
        else:
            bot_move = random.choice(legal_moves)
        
        # Apply bot move
        board.push(bot_move)
        fen_after_bot = board.fen()
        bot_move_uci = bot_move.uci()
        
        print(f"[Bot] Player move: {player_move} → Bot move: {bot_move_uci}")
        
        return fen_after_player, bot_move_uci, fen_after_bot
        
    except Exception as e:
        print(f"[Bot] Error calculating move: {e}", file=sys.stderr)
        return None, None, None


def handle_client(client_socket, addr):
    """Handle a single client connection"""
    print(f"[Bot] Connection from {addr}")
    
    try:
        # Receive request: "fen|player_move\n"
        data = client_socket.recv(4096).decode('utf-8').strip()
        
        if not data:
            print("[Bot] Empty request", file=sys.stderr)
            return
        
        print(f"[Bot] Received: {data}")
        
        # Parse request
        parts = data.split('|')
        if len(parts) < 1:
            print("[Bot] Invalid request format", file=sys.stderr)
            client_socket.sendall(b"ERROR|Invalid format\n")
            return
        
        fen = parts[0]
        player_move = parts[1] if len(parts) > 1 else ""
        
        # Calculate bot move
        fen_after_player, bot_move, fen_after_bot = calculate_bot_move(fen, player_move)
        
        if not bot_move:
            client_socket.sendall(b"ERROR|Failed to calculate move\n")
            return
        
        # Send response: "fen_after_player|bot_move|fen_after_bot\n"
        response = f"{fen_after_player}|{bot_move}|{fen_after_bot}\n"
        client_socket.sendall(response.encode('utf-8'))
        
        print(f"[Bot] Sent response: {bot_move}")
        
    except Exception as e:
        print(f"[Bot] Error handling client: {e}", file=sys.stderr)
        try:
            client_socket.sendall(b"ERROR|Internal error\n")
        except:
            pass
    finally:
        client_socket.close()


def main():
    """Main server loop"""
    print("=" * 50)
    print("     Chess Bot Server Starting...")
    print("=" * 50)
    print(f"[Bot] Listening on {HOST}:{PORT}")
    print("Waiting for connections from C server...")
    print()
    
    # Create socket
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server_socket.bind((HOST, PORT))
        server_socket.listen(5)
        
        print(f"✅ Bot server ready on {HOST}:{PORT}")
        print()
        
        while True:
            # Accept connection
            client_socket, addr = server_socket.accept()
            
            # Handle client (blocking - simple single-threaded for now)
            handle_client(client_socket, addr)
            
    except KeyboardInterrupt:
        print("\n[Bot] Shutting down...")
    except Exception as e:
        print(f"[Bot] Server error: {e}", file=sys.stderr)
    finally:
        server_socket.close()
        print("[Bot] Server stopped")


if __name__ == '__main__':
    main()
