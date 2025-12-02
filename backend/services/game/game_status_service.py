# game/game_status_service.py - Game status queries
from typing import Dict, Any
from services.bridge import get_c_server_bridge


class GameStatusService:
    """Handles game status queries"""
    
    def __init__(self):
        self.bridge = get_c_server_bridge()
    
    def get_match_status(self, match_id: int) -> Dict[str, Any]:
        """
        Get current status of a match
        
        Args:
            match_id: ID of the match
            
        Returns:
            {
                "success": bool,
                "match_id": int,
                "status": str,
                "white_online": bool,
                "black_online": bool,
                "current_fen": str,
                "new_match_id": int (optional - for rematch),
                "error": str
            }
        """
        try:
            command = f"GET_MATCH_STATUS|{match_id}"
            response = self.bridge.send_command(command)

            if response and response.startswith("MATCH_STATUS|"):
                # Format: MATCH_STATUS|match_id|status|white_online|black_online|fen|rematch_id
                parts = response.split("|")
                result = {
                    "success": True,
                    "match_id": int(parts[1]),
                    "status": parts[2],
                    "white_online": parts[3] == "1",
                    "black_online": parts[4] == "1",
                    "current_fen": parts[5]
                }
                
                # Parse rematch_id from C server response (if available)
                if len(parts) > 6:
                    rematch_id = int(parts[6])
                    if rematch_id > 0:
                        result["new_match_id"] = rematch_id
                        print(f"[Status] Rematch detected: new_match_id={rematch_id}")
                        # Don't need database check if C server provided rematch_id
                        return result
                
                # Fallback: Check database only if C server didn't provide rematch_id
                # (This requires psycopg2 to be installed)
                try:
                    self._check_database_for_rematch(match_id, result)
                except Exception as e:
                    # Silently fail - rematch detection is optional
                    pass
                
                return result
            else:
                return {
                    "success": False,
                    "error": response or "Match not found"
                }

        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }
    
    def _check_database_for_rematch(self, match_id: int, result: Dict[str, Any]):
        """Check database for rematch if not found in C server response"""
        if "new_match_id" in result:
            return  # Already has rematch_id
        
        try:
            import psycopg2
            conn = psycopg2.connect(
                host="localhost",
                database="chess_db",
                user="postgres",
                password="123"
            )
            cur = conn.cursor()
            
            # Find newer match with same players
            cur.execute("""
                SELECT m2.match_id 
                FROM match_game m1
                JOIN match_player mp1_w ON m1.match_id = mp1_w.match_id AND mp1_w.color = 'white'
                JOIN match_player mp1_b ON m1.match_id = mp1_b.match_id AND mp1_b.color = 'black'
                JOIN match_game m2 ON m2.match_id > m1.match_id AND m2.type = 'pvp'
                JOIN match_player mp2_w ON m2.match_id = mp2_w.match_id AND mp2_w.color = 'white'
                JOIN match_player mp2_b ON m2.match_id = mp2_b.match_id AND mp2_b.color = 'black'
                WHERE m1.match_id = %s
                AND (
                    (mp2_w.user_id = mp1_w.user_id AND mp2_b.user_id = mp1_b.user_id)
                    OR (mp2_w.user_id = mp1_b.user_id AND mp2_b.user_id = mp1_w.user_id)
                )
                ORDER BY m2.match_id DESC
                LIMIT 1
            """, (match_id,))
            
            row = cur.fetchone()
            if row:
                result["new_match_id"] = row[0]
                print(f"[Status] Rematch found in database: new_match_id={row[0]}")
            
            cur.close()
            conn.close()
        except Exception as db_err:
            print(f"[WARNING] Could not check for rematch in database: {db_err}")
