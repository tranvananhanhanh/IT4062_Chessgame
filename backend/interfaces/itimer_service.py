from abc import ABC, abstractmethod
from typing import Callable, Any

class ITimerService(ABC):
    """Interface for timer service operations"""

    @abstractmethod
    def start_game_timer(self, match_id: int, duration_minutes: int,
                        callback: Callable[[int], Any]) -> bool:
        """Start a timer for a game"""
        pass

    @abstractmethod
    def stop_game_timer(self, match_id: int) -> bool:
        """Stop a game timer"""
        pass

    @abstractmethod
    def get_remaining_time(self, match_id: int) -> int:
        """Get remaining time in seconds for a game"""
        pass

    @abstractmethod
    def pause_timer(self, match_id: int) -> bool:
        """Pause a game timer"""
        pass

    @abstractmethod
    def resume_timer(self, match_id: int) -> bool:
        """Resume a paused game timer"""
        pass