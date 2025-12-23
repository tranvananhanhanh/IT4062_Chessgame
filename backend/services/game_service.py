# game_service.py - Facade pattern for game services
"""
Refactored Game Service following SOLID principles

This is a Facade that delegates to specialized services:
- PvPGameService: Player vs Player game logic
- BotGameService: Bot game logic  
- GameControlService: Game control operations (pause, resume, draw, rematch)
- GameStatusService: Status queries

Benefits:
✅ Single Responsibility: Each service has ONE clear purpose
✅ Easy Testing: Can mock individual services
✅ Maintainability: Changes isolated to specific modules
✅ Reusability: Services can be used independently
"""

from typing import Dict, Any
from interfaces.igame_service import IGameService
from services.game import (
    PvPGameService,
    BotGameService,
    GameControlService,
    GameStatusService
)


class GameService(IGameService):
    """
    Facade for game-related operations
    Delegates to specialized service classes
    """

    def __init__(self):
        # Initialize specialized services
        self.pvp_service = PvPGameService()
        self.bot_service = BotGameService()
        self.control_service = GameControlService()
        self.status_service = GameStatusService()

    # ==================== PvP Game Methods ====================
    
    def start_game(self, player1_id: int, player1_name: str,
                   player2_id: int, player2_name: str,
                   time_limit_minutes: int = 30) -> Dict[str, Any]:
        """Start a new 1v1 game - delegates to PvPGameService"""
        return self.pvp_service.start_game(
            player1_id, player1_name,
            player2_id, player2_name,
            time_limit_minutes
        )
    
    def join_game(self, match_id: int, player_id: int, player_name: str) -> Dict[str, Any]:
        """Join an existing game - delegates to PvPGameService"""
        return self.pvp_service.join_game(match_id, player_id, player_name)
    
    def make_move(self, match_id: int, player_id: int,
                  from_square: str, to_square: str) -> Dict[str, Any]:
        """Make a move in PvP game - delegates to PvPGameService"""
        return self.pvp_service.make_move(match_id, player_id, from_square, to_square)
    
    def surrender_game(self, match_id: int, player_id: int) -> Dict[str, Any]:
        """Surrender the current game - delegates to PvPGameService"""
        return self.pvp_service.surrender_game(match_id, player_id)

    # ==================== Bot Game Methods ====================
    
    def start_bot_game(self, user_id: int) -> Dict[str, Any]:
        """Initialize a bot game - delegates to BotGameService"""
        return self.bot_service.start_bot_game(user_id)
    
    def make_bot_move(self, match_id: int, move: str) -> Dict[str, Any]:
        """Make a move in bot game - delegates to BotGameService"""
        return self.bot_service.make_bot_move(match_id, move)

    # ==================== Game Control Methods ====================
    
    def game_control(self, match_id: int, player_id: int, action: str) -> Dict[str, Any]:
        """Handle game control actions - delegates to GameControlService"""
        return self.control_service.game_control(match_id, player_id, action)

    # ==================== Status Query Methods ====================
    
    def get_match_status(self, match_id: int) -> Dict[str, Any]:
        """Get current match status - delegates to GameStatusService"""
        return self.status_service.get_match_status(match_id)


# ==================== Global Instance ====================

# Singleton instance
game_service = GameService()


def get_game_service() -> GameService:
    """Get the global game service instance"""
    return game_service
