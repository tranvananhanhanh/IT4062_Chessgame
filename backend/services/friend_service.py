from services.socket_bridge import send_request

class FriendService:
    def send_friend_request(self, user_id, friend_id):
        cmd = f"FRIEND_REQUEST|{user_id}|{friend_id}"
        resp = send_request(cmd)
        if resp and resp.startswith("FRIEND_REQUESTED"):
            return {"success": True, "message": "Friend request sent"}
        else:
            return {"success": False, "error": resp or "Failed to send friend request"}

    def accept_friend_request(self, user_id, friend_id):
        cmd = f"FRIEND_ACCEPT|{user_id}|{friend_id}"
        resp = send_request(cmd)
        if resp and resp.startswith("FRIEND_ACCEPTED"):
            return {"success": True, "message": "Friend request accepted"}
        else:
            return {"success": False, "error": resp or "Failed to accept friend request"}

    def decline_friend_request(self, user_id, friend_id):
        cmd = f"FRIEND_DECLINE|{user_id}|{friend_id}"
        resp = send_request(cmd)
        if resp and resp.startswith("FRIEND_DECLINED"):
            return {"success": True, "message": "Friend request declined"}
        else:
            return {"success": False, "error": resp or "Failed to decline friend request"}

    def list_friends(self, user_id):
        cmd = f"FRIEND_LIST|{user_id}"
        resp = send_request(cmd)
        if resp and resp.startswith("FRIEND_LIST|"):
            ids = resp.strip().split('|')[1]
            friend_ids = ids.split(',') if ids else []
            return {"success": True, "friends": friend_ids}
        else:
            return {"success": False, "error": resp or "Failed to get friend list", "friends": []}

    def list_friend_requests(self, user_id):
        cmd = f"FRIEND_REQUESTS|{user_id}"
        resp = send_request(cmd)
        if resp and resp.startswith("FRIEND_REQUESTS|"):
            ids = resp.strip().split('|')[1]
            request_ids = ids.split(',') if ids else []
            return {"success": True, "requests": request_ids}
        else:
            return {"success": False, "error": resp or "Failed to get friend requests", "requests": []}

# Singleton instance
def get_friend_service():
    global _friend_service
    try:
        return _friend_service
    except NameError:
        _friend_service = FriendService()
        return _friend_service
