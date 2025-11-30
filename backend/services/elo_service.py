from services.socket_bridge import get_c_bridge
from typing import Dict, Any


class EloService:
    """Service class for ELO rating operations"""

    def __init__(self):
        self.c_bridge = get_c_bridge()

    def get_leaderboard(self, limit: int = 20) -> Dict[str, Any]:
        """
        Get ELO leaderboard (top players)
        Returns: {"success": bool, "leaderboard": list, "error": str}
        """
        try:
            command = f"GET_LEADERBOARD|{limit}"
            response = self.c_bridge.send_command(command)

            if not response:
                return {
                    "success": False,
                    "error": "No response from server",
                    "leaderboard": []
                }

            response = response.strip()

            if response.startswith("LEADERBOARD|"):
                parts = response.split("|")
                count = int(parts[1])
                
                leaderboard = []
                idx = 2
                for _ in range(count):
                    if idx + 6 < len(parts):
                        player = {
                            "rank": int(parts[idx]),
                            "user_id": int(parts[idx + 1]),
                            "username": parts[idx + 2],
                            "elo": int(parts[idx + 3]),
                            "wins": int(parts[idx + 4]),
                            "losses": int(parts[idx + 5]),
                            "draws": int(parts[idx + 6])
                        }
                        leaderboard.append(player)
                        idx += 7

                return {
                    "success": True,
                    "leaderboard": leaderboard
                }
            elif response.startswith("ERROR|"):
                return {
                    "success": False,
                    "error": response.split("|", 1)[1],
                    "leaderboard": []
                }
            else:
                return {
                    "success": False,
                    "error": f"Unexpected response: {response}",
                    "leaderboard": []
                }

        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "leaderboard": []
            }

    def get_elo_history(self, user_id: int) -> Dict[str, Any]:
        """
        Get ELO history for a user
        Returns: {"success": bool, "elo_history": list, "error": str}
        """
        try:
            command = f"GET_ELO_HISTORY|{user_id}"
            response = self.c_bridge.send_command(command)

            if not response:
                return {
                    "success": False,
                    "error": "No response from server",
                    "elo_history": []
                }

            response = response.strip()

            if response.startswith("ELO_HISTORY|"):
                parts = response.split("|")
                count = int(parts[1])
                
                elo_history = []
                idx = 2
                for _ in range(count):
                    if idx + 4 < len(parts):
                        entry = {
                            "match_id": int(parts[idx]),
                            "old_elo": int(parts[idx + 1]),
                            "new_elo": int(parts[idx + 2]),
                            "change": int(parts[idx + 3]),
                            "date": parts[idx + 4]
                        }
                        elo_history.append(entry)
                        idx += 5

                return {
                    "success": True,
                    "elo_history": elo_history
                }
            elif response.startswith("ERROR|"):
                return {
                    "success": False,
                    "error": response.split("|", 1)[1],
                    "elo_history": []
                }
            else:
                return {
                    "success": False,
                    "error": f"Unexpected response: {response}",
                    "elo_history": []
                }

        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "elo_history": []
            }


# Global instance
_elo_service = EloService()


def get_elo_service() -> EloService:
    """Get the global ELO service instance"""
    return _elo_service
