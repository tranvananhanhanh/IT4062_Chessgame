import json
import os
from pathlib import Path

class CredentialsCache:
    """Cache system để lưu/tải username và password đã mã hóa cơ bản"""
    
    CACHE_DIR = Path.home() / ".chess_client"
    CACHE_FILE = CACHE_DIR / "credentials.json"
    
    def __init__(self):
        self.CACHE_DIR.mkdir(parents=True, exist_ok=True)
    
    def save_credentials(self, username: str, password: str) -> bool:
        """Lưu credentials vào cache"""
        try:
            data = {
                "username": username,
                "password": self._simple_encode(password)
            }
            with open(self.CACHE_FILE, 'w') as f:
                json.dump(data, f)
            return True
        except Exception as e:
            print(f"[ERROR] Không thể lưu credentials: {e}")
            return False
    
    def load_credentials(self) -> dict:
        """Tải credentials từ cache"""
        try:
            if self.CACHE_FILE.exists():
                with open(self.CACHE_FILE, 'r') as f:
                    data = json.load(f)
                return {
                    "username": data.get("username", ""),
                    "password": self._simple_decode(data.get("password", ""))
                }
        except Exception as e:
            print(f"[ERROR] Không thể tải credentials: {e}")
        return {"username": "", "password": ""}
    
    def clear_credentials(self) -> bool:
        """Xóa credentials từ cache"""
        try:
            if self.CACHE_FILE.exists():
                self.CACHE_FILE.unlink()
            return True
        except Exception as e:
            print(f"[ERROR] Không thể xóa credentials: {e}")
            return False
    
    def has_cached_credentials(self) -> bool:
        """Kiểm tra xem có credentials trong cache không"""
        return self.CACHE_FILE.exists()
    
    @staticmethod
    def _simple_encode(text: str) -> str:
        """Mã hóa đơn giản (không dùng cho production)"""
        return ''.join(chr(ord(c) + 7) for c in text)
    
    @staticmethod
    def _simple_decode(text: str) -> str:
        """Giải mã đơn giản"""
        return ''.join(chr(ord(c) - 7) for c in text)
