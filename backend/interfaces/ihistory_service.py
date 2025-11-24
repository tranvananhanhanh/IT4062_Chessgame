from abc import ABC, abstractmethod
from typing import Dict, Any, List

class IHistoryService(ABC):
    """Interface for history service operations"""

    @abstractmethod
    def get_game_history(self, user_id: int) -> Dict[str, Any]:
        """Get game history for a user"""
        pass

    @abstractmethod
    def get_game_replay(self, match_id: int) -> Dict[str, Any]:
        """Get detailed replay of a match"""
        pass

    @abstractmethod
    def get_player_stats(self, user_id: int) -> Dict[str, Any]:
        """Get player statistics"""
        pass