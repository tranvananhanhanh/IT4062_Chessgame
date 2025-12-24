# socket_bridge.py - Legacy compatibility wrapper
"""
DEPRECATED: This file is kept for backward compatibility only.
New code should use services.bridge.get_c_server_bridge() instead.

This file will be removed in future versions. Please update your imports:
- OLD: from services.socket_bridge import get_c_bridge
- NEW: from services.bridge import get_c_server_bridge
"""

from services.bridge import get_c_server_bridge


def get_c_bridge():
    """
    DEPRECATED: Use get_c_server_bridge() from services.bridge instead
    
    This function exists only for backward compatibility with history_service.py
    """
    return get_c_server_bridge()


def send_request(cmd: str):
    """
    Gửi lệnh sang C server và nhận phản hồi.
    """
    bridge = get_c_server_bridge()
    return bridge.send_command(cmd)


# Alias for backward compatibility
CSocketBridge = get_c_server_bridge().__class__
