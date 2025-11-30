from services.socket_bridge import get_c_bridge


class LoginService:
    """Service for login operations via C server"""

    def __init__(self):
        self.c_bridge = get_c_bridge()

    def login(self, username: str, password: str):
        """
        Authenticate user against C server / database.
        Returns dict: {success, user_id, username, elo, error}
        """
        try:
            command = f"LOGIN|{username}|{password}"
            response = self.c_bridge.send_command(command)

            if not response:
                return {"success": False, "error": "No response from auth server"}

            response = response.strip()

            if response.startswith("LOGIN_SUCCESS|"):
                parts = response.split("|")
                if len(parts) >= 4:
                    return {
                        "success": True,
                        "user_id": int(parts[1]),
                        "username": parts[2],
                        "elo": int(parts[3]) if parts[3].isdigit() else parts[3],
                        "message": "Login successful",
                    }
                return {"success": False, "error": "Malformed success response"}

            if response.startswith("ERROR|"):
                return {"success": False, "error": response.split("|", 1)[1]}

            return {"success": False, "error": f"Unexpected response: {response}"}

        except Exception as exc:
            return {"success": False, "error": str(exc)}


_login_service = LoginService()


def get_login_service() -> LoginService:
    return _login_service
