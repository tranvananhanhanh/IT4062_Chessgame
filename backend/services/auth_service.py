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
        2. Nếu OK -> kiểm tra username/email trùng trong DB
        3. Nếu OK -> tạo user
        """
        # Nếu email None thì cho thành chuỗi rỗng để tránh 'None' literal
        email_for_cmd = email or ""

        # 1. Gửi validate sang C
        command = f"REGISTER_VALIDATE|{username}|{password}|{email_for_cmd}"
        response = self.c_bridge.send_command(command)

        if not response:
            return {"success": False, "error": "No response from C server"}

        if response.startswith("REGISTER_ERROR|"):
            msg = response.split("|", 1)[1]
            return {"success": False, "error": msg}

        if not response.startswith("REGISTER_OK"):
            return {
                "success": False,
                "error": f"Unexpected response from C server: {response}"
            }

        # 2. Check trùng username
        existing = User.query.filter_by(name=username).first()
        if existing:
            return {"success": False, "error": "Username already exists"}

        # 3. Check trùng email (nếu gửi lên)
        if email:
            existing_email = User.query.filter_by(email=email).first()
            if existing_email:
                return {"success": False, "error": "Email already exists"}

        # 4. Tạo user
        password_hash = generate_password_hash(password)
        user = User(name=username, email=email, password_hash=password_hash)
        db.session.add(user)
        try:
            db.session.commit()
        except Exception as e:
            db.session.rollback()
            return {"success": False, "error": f"Database error: {str(e)}"}

        return {
            "success": True,
            "user_id": user.user_id,
            "username": user.name,
            "email": user.email,
            "elo_point": user.elo_point
        }

# Global instance
_auth_service = AuthService()

def get_auth_service() -> AuthService:
    return _auth_service
