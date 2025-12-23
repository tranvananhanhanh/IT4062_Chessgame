# game/game_control_service.py - Game control operations (pause, resume, draw, rematch)
from typing import Dict, Any
from services.bridge import get_c_server_bridge


class GameControlService:
    """Handles game control operations like pause, resume, draw, and rematch"""
    
    def __init__(self):
        self.bridge = get_c_server_bridge()
    
    def game_control(self, match_id: int, player_id: int, action: str) -> Dict[str, Any]:
        """
        Handle game control actions
        
        Args:
            match_id: ID of the match
            player_id: Player performing the action
            action: Action to perform (PAUSE, RESUME, DRAW, DRAW_ACCEPT, DRAW_DECLINE,
                                      REMATCH, REMATCH_ACCEPT, REMATCH_DECLINE)
        
        Returns:
            Dict with success status, message, and action-specific data
        """
        try:
            # Build command based on action
            if action in ['REMATCH', 'REMATCH_ACCEPT', 'REMATCH_DECLINE',
                         'DRAW', 'DRAW_ACCEPT', 'DRAW_DECLINE',
                         'PAUSE', 'RESUME']:
                command = f"{action}|{match_id}|{player_id}"
            else:
                return {"success": False, "error": f"Unknown action: {action}"}

            print(f"[GameControl] Sending: {command}")
            response = self.bridge.send_command(command)
            print(f"[GameControl] Received: {response}")

            # Parse response based on action type
            return self._parse_response(response, action, player_id)

        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def _parse_response(self, response: str, action: str, player_id: int) -> Dict[str, Any]:
        """Parse C server response based on action type"""
        
        if not response:
            return {"success": False, "error": "No response from server"}
        
        # Rematch responses
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
        
        # Draw responses
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
        
        # Pause/Resume responses
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
        
        # Error response
        elif response.startswith("ERROR"):
            error_msg = response.replace("ERROR|", "")
            return {
                "success": False,
                "error": error_msg
            }
        
        # Unexpected response
        else:
            print(f"[WARNING] Unexpected response format: {response}")
            return {
                "success": False,
                "error": f"Unexpected response from server: {response}"
            }
