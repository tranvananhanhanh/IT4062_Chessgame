from typing import Dict, Any, Optional
from werkzeug.security import generate_password_hash
from services.socket_bridge import get_c_bridge
from models import db, User

class AuthService:
    def __init__(self):
        self.c_bridge = get_c_bridge()

    def register(self, username: str, password: str, email: Optional[str] = None) -> Dict[str, Any]:
        """
        1. Gửi REGISTER_VALIDATE|username|password|email sang C server
        2. Nếu OK -> trả về kết quả, KHÔNG truy cập DB trực tiếp ở Python nữa
        """
        email_for_cmd = email or ""
        command = f"REGISTER_VALIDATE|{username}|{password}|{email_for_cmd}"
        response = self.c_bridge.send_command(command)

        if not response:
            return {"success": False, "error": "No response from C server"}

        if response.startswith("REGISTER_ERROR|"):
            msg = response.split("|", 1)[1]
            return {"success": False, "error": msg}

        if response.startswith("REGISTER_OK|"):
            # Lấy user_id từ response nếu có
            parts = response.strip().split('|')
            user_id = int(parts[1]) if len(parts) > 1 else None
            return {
                "success": True,
                "user_id": user_id,
                "username": username,
                "email": email,
                # Không lấy elo_point từ DB nữa, có thể lấy từ C server nếu cần
            }

        return {
            "success": False,
            "error": f"Unexpected response from C server: {response}"
        }

# Global instance
_auth_service = AuthService()

def get_auth_service() -> AuthService:
    return _auth_service
