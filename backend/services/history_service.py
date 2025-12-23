from services.socket_bridge import get_c_bridge
import json
from typing import Dict, Any

class HistoryService:
    """Service class for game history and statistics operations"""

    def __init__(self):
        self.c_bridge = get_c_bridge()

    def get_game_history(self, user_id: int) -> Dict[str, Any]:
        """
        Get game history for a user
        Returns: {"success": bool, "history": list, "error": str}
        """
        try:
            command = f"GET_HISTORY|{user_id}"
            response = self.c_bridge.send_command(command)

            if response and response.startswith("HISTORY|"):
                history_json = response.split("|", 1)[1]
                history_data = json.loads(history_json)

                return {
                    "success": True,
                    "history": history_data
                }
            else:
                return {
                    "success": False,
                    "error": "Failed to get history",
                    "history": []
                }

        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "history": []
            }

    def get_game_replay(self, match_id: int) -> Dict[str, Any]:
        """
        Get detailed replay of a specific match
        Returns: {"success": bool, "replay": dict, "error": str}
        """
        try:
            command = f"GET_REPLAY|{match_id}"
            response = self.c_bridge.send_command(command)

            if response and response.startswith("REPLAY|"):
                replay_json = response.split("|", 1)[1]
                replay_data = json.loads(replay_json)

                return {
                    "success": True,
                    "replay": replay_data
                }
            else:
                return {
                    "success": False,
                    "error": "Failed to get replay",
                    "replay": {}
                }

        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "replay": {}
            }

    def get_player_stats(self, user_id: int) -> Dict[str, Any]:
        """
        Get player statistics
        Returns: {"success": bool, "stats": dict, "error": str}
        """
        try:
            command = f"GET_STATS|{user_id}"
            response = self.c_bridge.send_command(command)

            if response and response.startswith("STATS|"):
                stats_json = response.split("|", 1)[1]
                stats_data = json.loads(stats_json)

                return {
                    "success": True,
                    "stats": stats_data
                }
            else:
                return {
                    "success": False,
                    "error": "Failed to get stats",
                    "stats": {}
                }

        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "stats": {}
            }

# Global instance
history_service = HistoryService()

def get_history_service() -> HistoryService:
    """Get the global history service instance"""
    return history_service