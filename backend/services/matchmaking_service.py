from .socket_bridge import send_request


def _ensure_response(cmd: str) -> str:
    resp = send_request(cmd)

    if resp is None:
        raise Exception(f"No response from game server for command: {cmd}")

    resp = resp.strip()
    if not resp:
        raise Exception(f"Empty response from game server for command: {cmd}")

    if resp.startswith("ERROR|") or resp.startswith("ERROR "):
        raise Exception(resp)

    return resp


def join_matchmaking(user_id: str, elo: int, time_mode: str = "BLITZ"):
    # Dùng '|' đúng style protocol_handle_command
    cmd = f"MMJOIN|{user_id}|{elo}|{time_mode}"
    resp = _ensure_response(cmd)
    parts = resp.split()

    if len(parts) == 0:
        raise Exception(f"Invalid response: {resp}")

    code = parts[0]

    if code == "QUEUED":
        return {"status": "queued"}

    if code == "MATCHED":
        if len(parts) < 5:
            raise Exception(f"Invalid MATCHED response: {resp}")
        _, match_id, white_id, black_id, my_color = parts
        opponent = white_id if my_color == "black" else black_id
        return {
            "status": "matched",
            "match_id": match_id,
            "opponent_id": opponent,
            "color": my_color,
        }

    if code.startswith("ERROR"):
        msg = " ".join(parts[1:]) if len(parts) > 1 else "Unknown matchmaking error"
        raise Exception(msg)

    raise Exception(f"Invalid response: {resp}")


def check_match_status(user_id: str):
    cmd = f"MMSTATUS|{user_id}"
    resp = _ensure_response(cmd)
    parts = resp.split()

    if len(parts) == 0:
        raise Exception(f"Invalid response: {resp}")

    code = parts[0]

    if code == "WAITING":
        return {"status": "waiting"}

    if code == "MATCHED":
        if len(parts) < 5:
            raise Exception(f"Invalid MATCHED response: {resp}")
        _, match_id, white_id, black_id, my_color = parts
        opponent = white_id if my_color == "black" else black_id
        return {
            "status": "matched",
            "match_id": match_id,
            "opponent_id": opponent,
            "color": my_color,
        }

    if code == "NOTFOUND":
        return {"status": "not_found"}

    if code.startswith("ERROR"):
        msg = " ".join(parts[1:]) if len(parts) > 1 else "Unknown matchmaking error"
        raise Exception(msg)

    raise Exception(f"Invalid response: {resp}")


def cancel_matchmaking(user_id: str):
    cmd = f"MMCANCEL|{user_id}"
    resp = _ensure_response(cmd)
    parts = resp.split()

    if len(parts) == 0:
        raise Exception(f"Invalid response: {resp}")

    code = parts[0]

    if code == "CANCELED":
        return {"status": "canceled"}

    if code == "NOTFOUND":
        return {"status": "not_found"}

    if code.startswith("ERROR"):
        msg = " ".join(parts[1:]) if len(parts) > 1 else "Unknown matchmaking error"
        raise Exception(msg)

    raise Exception(f"Invalid response: {resp}")