# game/bot_game_service.py - Bot game logic
from typing import Dict, Any
from services.bridge import get_c_server_bridge


class BotGameService:
    """Handles Bot game operations"""
    
    def __init__(self):
        self.bridge = get_c_server_bridge()
    
    def start_bot_game(self, user_id: int) -> Dict[str, Any]:
        """
        Initialize a bot game
        
        Args:
            user_id: Player's user ID
            
        Returns:
            {"success": bool, "match_id": int, "fen": str, "error": str}
        """
        try:
            command = f"MODE_BOT|{user_id}"
            response = self.bridge.send_command(command)
            
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
    
    def make_bot_move(self, match_id: int, move: str) -> Dict[str, Any]:
        """
        Make a move in bot game and get bot's response
        
        Args:
            match_id: ID of the bot match
            move: Move notation (e.g., "e2e4")
            
        Returns:
            {
                "success": bool,
                "fen_after_player": str,
                "bot_move": str,
                "fen_after_bot": str,
                "status": str,
                "game_ended": bool (optional),
                "reason": str (optional),
                "result": str (optional),
                "error": str
            }
        """
        try:
            command = f"BOT_MOVE|{match_id}|{move}"
            response = self.bridge.send_command(command)
            
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
