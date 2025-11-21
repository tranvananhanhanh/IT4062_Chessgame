from typing import Dict, Any
from interfaces.igame_service import IGameService
from services.socket_bridge import get_c_bridge


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

            if response and response.startswith("SURRENDER_SUCCESS"):
                return {
                    "success": True,
                    "message": "Surrendered successfully"
                }
            else:
                return {
                    "success": False,
                    "error": "Failed to surrender"
                }

        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }


# Global instance
game_service = GameService()

def get_game_service() -> GameService:
    """Get the global game service instance"""
    return game_service