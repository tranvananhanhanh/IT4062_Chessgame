from abc import ABC, abstractmethod
from typing import Dict, Any, Optional

class IGameService(ABC):
    """Interface for game service operations"""

    @abstractmethod
    def start_game(self, player1_id: int, player1_name: str,
                   player2_id: int, player2_name: str) -> Dict[str, Any]:
        """Start a new game"""
        pass

    @abstractmethod
    def make_move(self, match_id: int, player_id: int,
                  from_square: str, to_square: str) -> Dict[str, Any]:
        """Make a move in the game"""
        pass

    @abstractmethod
    def join_game(self, match_id: int, player_id: int, player_name: str) -> Dict[str, Any]:
        """Join an existing game"""
        pass

    @abstractmethod
    def surrender_game(self, match_id: int, player_id: int) -> Dict[str, Any]:
        """Surrender the game"""
        pass