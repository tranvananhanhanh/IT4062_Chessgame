from typing import Dict, Any
from interfaces.igame_service import IGameService
from services.socket_bridge import get_c_bridge
import socket
import threading


class CServerBridge:
    def __init__(self, host='localhost', port=8888):
        self.host = host
        self.port = port
        self.sock = None
        self.lock = threading.Lock()
        
    def connect(self):
        """Establish connection to C server"""
        try:
            if self.sock:
                try:
                    self.sock.close()
                except:
                    pass
            
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            # ✅ Add socket timeout to prevent hanging
            self.sock.settimeout(5.0)  # 5 second timeout
            self.sock.connect((self.host, self.port))
            print(f"[Bridge] Connected to C server at {self.host}:{self.port}")
            return True
        except socket.timeout:
            print(f"[Bridge] Connection timeout to C server at {self.host}:{self.port}")
            return False
        except Exception as e:
            print(f"Failed to connect to C server at {self.host}:{self.port}")
            print(f"Error: {e}")
            return False
    
    def send_command(self, command):
        """Send command to C server and get response"""
        with self.lock:
            try:
                if not self.sock:
                    if not self.connect():
                        return None
                
                # Send command
                print(f"[DEBUG] Sent: {command.strip()}")
                self.sock.sendall((command + '\n').encode())
                
                # ✅ Receive response with timeout
                try:
                    response = self.sock.recv(4096).decode().strip()
                    print(f"[DEBUG] Received: {response}")
                    return response
                except socket.timeout:
                    print("[Bridge] Timeout waiting for response from C server")
                    # Try to reconnect for next time
                    self.connect()
                    return None
                
            except (BrokenPipeError, ConnectionResetError) as e:
                print(f"[Bridge] Connection broken: {e}, reconnecting...")
                if self.connect():
                    # Retry once
                    try:
                        self.sock.sendall((command + '\n').encode())
                        response = self.sock.recv(4096).decode().strip()
                        return response
                    except:
                        return None
                return None
            except Exception as e:
                print(f"[Bridge] Error sending command: {e}")
                import traceback
                traceback.print_exc()
                return None


class GameService(IGameService):
    """Service class for game-related operations following SOLID principles"""

    def __init__(self):
        self.c_bridge = get_c_bridge()

    def start_game(self, player1_id: int, player1_name: str,
                   player2_id: int, player2_name: str,
                   time_limit_minutes: int = 30) -> Dict[str, Any]:
        """
        Start a new 1v1 game with timer
        Returns: {"success": bool, "match_id": int, "initial_fen": str, "error": str}
        """
        try:
            command = f"START_MATCH|{player1_id}|{player1_name}|{player2_id}|{player2_name}"
            response = self.c_bridge.send_command(command)
            print(f"[DEBUG] START_MATCH command: {command}")
            print(f"[DEBUG] START_MATCH response: {response}")

            if response and response.startswith("MATCH_CREATED|"):
                parts = response.split("|")
                match_id = int(parts[1])
                fen = parts[2]

                return {
                    "success": True,
                    "match_id": match_id,
                    "initial_fen": fen,
                    "message": "Game started successfully"
                }
            else:
                return {
                    "success": False,
                    "error": response or "Unknown error from server"
                }

        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }

    def make_move(self, match_id: int, player_id: int,
                  from_square: str, to_square: str) -> Dict[str, Any]:
        """
        Make a move in the game
        Returns: {"success": bool, "new_fen": str, "game_status": str, "error": str}
        """
        try:
            command = f"MOVE|{match_id}|{player_id}|{from_square}|{to_square}"
            response = self.c_bridge.send_command(command)
            print(f"[DEBUG] MOVE command: {command}")
            print(f"[DEBUG] MOVE response: {response}")

            if response and response.startswith("MOVE_SUCCESS|"):
                parts = response.split("|")
                notation = parts[1]
                new_fen = parts[2]
                game_status = "playing"

                return {
                    "success": True,
                    "new_fen": new_fen,
                    "game_status": game_status,
                    "message": "Move executed successfully"
                }
            elif response and response.startswith("ERROR|"):
                return {
                    "success": False,
                    "error": response.split("|", 1)[1]
                }
            else:
                return {
                    "success": False,
                    "error": "Invalid move or server error"
                }

        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }

    def join_game(self, match_id: int, player_id: int, player_name: str) -> Dict[str, Any]:
        """
        Join an existing game
        Returns: {"success": bool, "match_id": int, "initial_fen": str, "error": str}
        """
        try:
            command = f"JOIN_MATCH|{match_id}|{player_id}|{player_name}"
            response = self.c_bridge.send_command(command)

            if response and response.startswith("MATCH_JOINED|"):
                parts = response.split("|")
                fen = parts[2]

                return {
                    "success": True,
                    "match_id": match_id,
                    "initial_fen": fen,
                    "message": "Joined game successfully"
                }
            else:
                return {
                    "success": False,
                    "error": response or "Failed to join game"
                }

        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }

    def get_match_status(self, match_id: int) -> Dict[str, Any]:
        """
        Get current status of a match
        Returns: {"success": bool, "match_id": int, "status": str, "white_online": bool, "black_online": bool, "current_fen": str, "new_match_id": int (optional)}
        """
        try:
            command = f"GET_MATCH_STATUS|{match_id}"
            response = self.c_bridge.send_command(command)

            if response and response.startswith("MATCH_STATUS|"):
                # Format: MATCH_STATUS|match_id|status|white_online|black_online|fen|rematch_id
                parts = response.split("|")
                result = {
                    "success": True,
                    "match_id": int(parts[1]),
                    "status": parts[2],
                    "white_online": parts[3] == "1",
                    "black_online": parts[4] == "1",
                    "current_fen": parts[5]
                }
                
                # ✅ Parse rematch_id from C server response (if available)
                if len(parts) > 6:
                    rematch_id = int(parts[6])
                    if rematch_id > 0:
                        result["new_match_id"] = rematch_id
                        print(f"[DEBUG] Rematch detected: new_match_id={rematch_id}")
                
                # ✅ Fallback: Check database if C server didn't provide rematch_id
                # Query database to find a newer match created after this one
                try:
                    import psycopg2
                    conn = psycopg2.connect(
                        host="localhost",
                        database="chess_db",
                        user="postgres",
                        password="123"
                    )
                    cur = conn.cursor()
                    
                    # Find newer match with same players
                    cur.execute("""
                        SELECT m2.match_id 
                        FROM match_game m1
                        JOIN match_player mp1_w ON m1.match_id = mp1_w.match_id AND mp1_w.color = 'white'
                        JOIN match_player mp1_b ON m1.match_id = mp1_b.match_id AND mp1_b.color = 'black'
                        JOIN match_game m2 ON m2.match_id > m1.match_id AND m2.type = 'pvp'
                        JOIN match_player mp2_w ON m2.match_id = mp2_w.match_id AND mp2_w.color = 'white'
                        JOIN match_player mp2_b ON m2.match_id = mp2_b.match_id AND mp2_b.color = 'black'
                        WHERE m1.match_id = %s
                        AND (
                            (mp2_w.user_id = mp1_w.user_id AND mp2_b.user_id = mp1_b.user_id)
                            OR (mp2_w.user_id = mp1_b.user_id AND mp2_b.user_id = mp1_w.user_id)
                        )
                        ORDER BY m2.match_id DESC
                        LIMIT 1
                    """, (match_id,))
                    
                    row = cur.fetchone()
                    if row:
                        result["new_match_id"] = row[0]
                    
                    cur.close()
                    conn.close()
                except Exception as db_err:
                    print(f"[WARNING] Could not check for rematch: {db_err}")
                
                return result
            else:
                return {
                    "success": False,
                    "error": response or "Match not found"
                }

        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }

    def surrender_game(self, match_id: int, player_id: int) -> Dict[str, Any]:
        """
        Surrender the current game
        Returns: {"success": bool, "error": str}
        """
        try:
            command = f"SURRENDER|{match_id}|{player_id}"
            response = self.c_bridge.send_command(command)
            print(f"[DEBUG] SURRENDER command: {command}")
            print(f"[DEBUG] SURRENDER response: {response}")

            # Accept any response containing SURRENDER_SUCCESS
            if response and "SURRENDER_SUCCESS" in response:
                return {
                    "success": True,
                    "message": "Surrendered successfully"
                }
            # Accept GAME_END as success (for compatibility)
            elif response and "GAME_END" in response:
                return {
                    "success": True,
                    "message": response
                }
            else:
                return {
                    "success": False,
                    "error": response or "Failed to surrender"
                }

        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }

    def game_control(self, match_id, player_id, action):
        """Handle game control actions including rematch"""
        try:
            if action in ['REMATCH', 'REMATCH_ACCEPT', 'REMATCH_DECLINE']:
                # ✅ Handle rematch actions
                command = f"{action}|{match_id}|{player_id}"
            elif action in ['DRAW', 'DRAW_ACCEPT', 'DRAW_DECLINE']:
                command = f"{action}|{match_id}|{player_id}"
            elif action in ['PAUSE', 'RESUME']:
                command = f"{action}|{match_id}|{player_id}"
            else:
                return {"success": False, "error": f"Unknown action: {action}"}

            print(f"[DEBUG] Sending: {command}")
            # ✅ FIX: Use self.c_bridge.send_command() instead of self._send_command()
            response = self.c_bridge.send_command(command)
            print(f"[DEBUG] Clean received: {response}")

            # ✅ Parse rematch responses (clean response only)
            if response.startswith("REMATCH_REQUESTED"):
                return {
                    "success": True,
                    "message": "Rematch requested",
                    "rematch_requested_by": player_id,
                    "waiting_for_response": True
                }
            elif response.startswith("REMATCH_ACCEPTED"):
                parts = response.split("|")
                new_match_id = int(parts[1]) if len(parts) > 1 else None
                return {
                    "success": True,
                    "message": "Rematch accepted",
                    "new_match_id": new_match_id,
                    "game_status": "playing"
                }
            elif response.startswith("REMATCH_DECLINED"):
                return {
                    "success": True,
                    "message": "Rematch declined",
                    "game_status": "finished"
                }
            elif response.startswith("ERROR"):
                error_msg = response.replace("ERROR|", "")
                return {
                    "success": False,
                    "error": error_msg
                }
            elif response.startswith("DRAW_REQUESTED"):
                return {
                    "success": True,
                    "message": "Draw requested",
                    "draw_requested_by": player_id,
                    "waiting_for_response": True
                }
            elif response.startswith("DRAW_ACCEPTED"):
                return {
                    "success": True,
                    "message": "Draw accepted",
                    "game_status": "finished"
                }
            elif response.startswith("DRAW_DECLINED"):
                return {
                    "success": True,
                    "message": "Draw declined",
                    "game_status": "playing"
                }
            elif response.startswith("GAME_PAUSED"):
                return {
                    "success": True,
                    "message": "Game paused",
                    "game_status": "paused"
                }
            elif response.startswith("GAME_RESUMED"):
                return {
                    "success": True,
                    "message": "Game resumed",
                    "game_status": "playing"
                }
            else:
                # ✅ Handle unexpected responses
                print(f"[WARNING] Unexpected response format: {response}")
                return {
                    "success": False,
                    "error": f"Unexpected response from server: {response}"
                }

        except Exception as e:
            return {"success": False, "error": str(e)}

    def start_bot_game(self, user_id):
        """
        Initialize a bot game
        """
        try:
            # Send MODE_BOT command to C server
            command = f"MODE_BOT|{user_id}"
            response = self.c_bridge.send_command(command)
            
            if response:
                response = response.strip()
                
                # Expected: BOT_MATCH_CREATED|match_id|fen
                if response.startswith("BOT_MATCH_CREATED"):
                    parts = response.split('|')
                    if len(parts) >= 3:
                        match_id = int(parts[1])
                        fen = parts[2]
                        
                        return {
                            "success": True,
                            "match_id": match_id,
                            "fen": fen,
                            "message": "Bot game created successfully"
                        }
                elif response.startswith("ERROR"):
                    error_msg = response.split('|', 1)[1] if '|' in response else "Unknown error"
                    return {
                        "success": False,
                        "error": error_msg
                    }
            
            return {
                "success": False,
                "error": "No response from game server"
            }
            
        except Exception as e:
            print(f"Error in start_bot_game: {e}")
            return {
                "success": False,
                "error": str(e)
            }
    
    def make_bot_move(self, match_id, move):
        """
        Make a move in bot game and get bot's response
        """
        try:
            # Send BOT_MOVE command to C server
            command = f"BOT_MOVE|{match_id}|{move}"
            response = self.c_bridge.send_command(command)
            
            if response:
                response = response.strip()
                
                # Expected: BOT_MOVE_RESULT|fen_after_player|bot_move|fen_after_bot|status
                if response.startswith("BOT_MOVE_RESULT"):
                    parts = response.split('|')
                    if len(parts) >= 5:
                        fen_after_player = parts[1]
                        bot_move = parts[2]
                        fen_after_bot = parts[3]
                        status = parts[4]
                        
                        return {
                            "success": True,
                            "fen_after_player": fen_after_player,
                            "bot_move": bot_move,
                            "fen_after_bot": fen_after_bot,
                            "status": status,
                            "message": f"Move executed: {move}, Bot responded: {bot_move}"
                        }
                # Handle GAME_END response: GAME_END|reason|result
                elif response.startswith("GAME_END"):
                    parts = response.split('|')
                    if len(parts) >= 3:
                        reason = parts[1]  # e.g., "insufficient_material", "checkmate", "stalemate"
                        result = parts[2]  # e.g., "draw", "white", "black"
                        
                        return {
                            "success": True,
                            "game_ended": True,
                            "reason": reason,
                            "result": result,
                            "status": result.upper() if result != "draw" else "DRAW",
                            "message": f"Game ended: {reason} - {result}"
                        }
                elif response.startswith("ERROR"):
                    error_msg = response.split('|', 1)[1] if '|' in response else "Unknown error"
                    return {
                        "success": False,
                        "error": error_msg
                    }
            
            return {
                "success": False,
                "error": "No response from game server"
            }
            
        except Exception as e:
            print(f"Error in make_bot_move: {e}")
            return {
                "success": False,
                "error": str(e)
            }


# Global instance
game_service = GameService()

def get_game_service() -> GameService:
    """Get the global game service instance"""
    return game_service
