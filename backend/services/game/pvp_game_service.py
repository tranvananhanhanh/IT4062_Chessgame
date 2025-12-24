# game/pvp_game_service.py - Player vs Player game logic
from typing import Dict, Any
from services.bridge import get_c_server_bridge


class PvPGameService:
    """Handles Player vs Player game operations"""
    
    def __init__(self):
        self.bridge = get_c_server_bridge()
    
    def start_game(self, player1_id: int, player1_name: str,
                   player2_id: int, player2_name: str,
                   time_limit_minutes: int = 30) -> Dict[str, Any]:
        """
        Start a new 1v1 game with timer
        
        Args:
            player1_id: First player's user ID
            player1_name: First player's username
            player2_id: Second player's user ID
            player2_name: Second player's username
            time_limit_minutes: Time limit in minutes
            
        Returns:
            {"success": bool, "match_id": int, "initial_fen": str, "error": str}
        """
        try:
            command = f"START_MATCH|{player1_id}|{player1_name}|{player2_id}|{player2_name}"
            response = self.bridge.send_command(command)

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
    
    def join_game(self, match_id: int, player_id: int, player_name: str) -> Dict[str, Any]:
        """
        Join an existing game
        
        Args:
            match_id: ID of match to join
            player_id: Player's user ID
            player_name: Player's username
            
        Returns:
            {"success": bool, "match_id": int, "initial_fen": str, "error": str}
        """
        try:
            command = f"JOIN_MATCH|{match_id}|{player_id}|{player_name}"
            response = self.bridge.send_command(command)

            # Accept both MATCH_JOINED and MATCH_STATUS as success
            if response and response.startswith("MATCH_JOINED|"):
                parts = response.split("|")
                fen = parts[2]

                return {
                    "success": True,
                    "match_id": match_id,
                    "initial_fen": fen,
                    "message": "Joined game successfully"
                }
            elif response and response.startswith("MATCH_STATUS|"):
                # Sometimes C server sends MATCH_STATUS after JOIN - parse it
                parts = response.split("|")
                if len(parts) >= 6:
                    fen = parts[5]
                    return {
                        "success": True,
                        "match_id": match_id,
                        "initial_fen": fen,
                        "message": "Joined game successfully (status verified)"
                    }
                
            # If neither format, it's an error
            return {
                "success": False,
                "error": response or "Failed to join game"
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
        
        Args:
            match_id: ID of the match
            player_id: Player making the move
            from_square: Starting square (e.g., "e2")
            to_square: Destination square (e.g., "e4")
            
        Returns:
            {"success": bool, "new_fen": str, "game_status": str, "error": str}
        """
        try:
            command = f"MOVE|{match_id}|{player_id}|{from_square}|{to_square}"
            response = self.bridge.send_command(command)

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
    
    def surrender_game(self, match_id: int, player_id: int) -> Dict[str, Any]:
        """
        Surrender the current game
        
        Args:
            match_id: ID of the match
            player_id: Player surrendering
            
        Returns:
            {"success": bool, "error": str}
        """
        try:
            command = f"SURRENDER|{match_id}|{player_id}"
            response = self.bridge.send_command(command)

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
