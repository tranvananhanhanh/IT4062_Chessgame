"""
Test script cho UC Surrender (Rời trận) và UC ELO
Chạy: python test_surrender_elo.py
Yêu cầu: Backend Flask đang chạy ở port 5002, C server đang chạy
"""

import requests
import json
import time

BASE_URL = "http://localhost:5002/api"

def print_response(name, response):
    print(f"\n{'='*50}")
    print(f"TEST: {name}")
    print(f"Status: {response.status_code}")
    try:
        print(f"Response: {json.dumps(response.json(), indent=2, ensure_ascii=False)}")
    except:
        print(f"Response: {response.text}")
    print('='*50)

def test_login(username, password):
    """Test đăng nhập"""
    response = requests.post(f"{BASE_URL}/auth/login", json={
        "username": username,
        "password": password
    })
    print_response(f"Login - {username}", response)
    return response.json() if response.status_code == 200 else None

def test_get_stats(user_id):
    """Lấy thống kê người chơi (bao gồm ELO)"""
    response = requests.get(f"{BASE_URL}/stats/{user_id}")
    print_response(f"Get Stats - User {user_id}", response)
    return response.json() if response.status_code == 200 else None

def test_get_leaderboard(limit=10):
    """Lấy bảng xếp hạng ELO"""
    response = requests.get(f"{BASE_URL}/leaderboard", params={"limit": limit})
    print_response(f"Leaderboard (top {limit})", response)
    return response.json() if response.status_code == 200 else None

def test_get_elo_history(user_id):
    """Lấy lịch sử thay đổi ELO"""
    response = requests.get(f"{BASE_URL}/elo/history/{user_id}")
    print_response(f"ELO History - User {user_id}", response)
    return response.json() if response.status_code == 200 else None

def test_start_game(p1_id, p1_name, p2_id, p2_name):
    """Bắt đầu trận đấu mới"""
    response = requests.post(f"{BASE_URL}/game/start", json={
        "player1_id": p1_id,
        "player1_name": p1_name,
        "player2_id": p2_id,
        "player2_name": p2_name
    })
    print_response(f"Start Game - {p1_name} vs {p2_name}", response)
    return response.json() if response.status_code == 200 else None

def test_surrender(match_id, player_id):
    """Test rời trận (surrender)"""
    response = requests.post(f"{BASE_URL}/game/surrender", json={
        "match_id": match_id,
        "player_id": player_id
    })
    print_response(f"Surrender - Match {match_id}, Player {player_id}", response)
    return response.json() if response.status_code == 200 else None

def test_make_move(match_id, player_id, from_sq, to_sq):
    """Test đi nước cờ"""
    response = requests.post(f"{BASE_URL}/game/move", json={
        "match_id": match_id,
        "player_id": player_id,
        "from_square": from_sq,
        "to_square": to_sq
    })
    print_response(f"Move - {from_sq} to {to_sq}", response)
    return response.json() if response.status_code == 200 else None

def run_surrender_test():
    """
    Test UC Surrender hoàn chỉnh:
    1. Lấy ELO ban đầu của 2 người chơi
    2. Tạo trận đấu
    3. Đi vài nước
    4. Player 1 surrender
    5. Kiểm tra ELO sau trận
    """
    print("\n" + "="*60)
    print("         TEST UC SURRENDER (RỜI TRẬN)")
    print("="*60)
    
    # Giả sử đã có user alice (id=1) và bob (id=2) trong DB
    player1_id = 1
    player1_name = "alice"
    player2_id = 2
    player2_name = "bob"
    
    # 1. Lấy ELO ban đầu
    print("\n[STEP 1] Lấy ELO ban đầu...")
    stats1_before = test_get_stats(player1_id)
    stats2_before = test_get_stats(player2_id)
    
    elo1_before = stats1_before.get("stats", {}).get("current_elo", 1000) if stats1_before else 1000
    elo2_before = stats2_before.get("stats", {}).get("current_elo", 1000) if stats2_before else 1000
    
    print(f"\nELO trước trận: {player1_name}={elo1_before}, {player2_name}={elo2_before}")
    
    # 2. Tạo trận đấu
    print("\n[STEP 2] Tạo trận đấu mới...")
    game_result = test_start_game(player1_id, player1_name, player2_id, player2_name)
    
    if not game_result or not game_result.get("success"):
        print("❌ Không thể tạo trận đấu!")
        return
    
    match_id = game_result.get("match_id")
    print(f"✅ Trận đấu được tạo: match_id = {match_id}")
    
    # 3. Đi vài nước (optional)
    print("\n[STEP 3] Đi vài nước cờ...")
    test_make_move(match_id, player1_id, "e2", "e4")  # White
    time.sleep(0.5)
    test_make_move(match_id, player2_id, "e7", "e5")  # Black
    time.sleep(0.5)
    
    # 4. Player 1 surrender
    print("\n[STEP 4] Player 1 (alice) rời trận...")
    surrender_result = test_surrender(match_id, player1_id)
    
    if surrender_result and surrender_result.get("success"):
        print(f"✅ Surrender thành công! Winner: {surrender_result.get('winner_id')}")
    else:
        print("❌ Surrender thất bại!")
        return
    
    # 5. Kiểm tra ELO sau trận
    print("\n[STEP 5] Kiểm tra ELO sau trận...")
    time.sleep(1)  # Đợi DB cập nhật
    
    stats1_after = test_get_stats(player1_id)
    stats2_after = test_get_stats(player2_id)
    
    elo1_after = stats1_after.get("stats", {}).get("current_elo", 1000) if stats1_after else 1000
    elo2_after = stats2_after.get("stats", {}).get("current_elo", 1000) if stats2_after else 1000
    
    print(f"\n{'='*50}")
    print("KẾT QUẢ ELO:")
    print(f"  {player1_name}: {elo1_before} -> {elo1_after} ({elo1_after - elo1_before:+d})")
    print(f"  {player2_name}: {elo2_before} -> {elo2_after} ({elo2_after - elo2_before:+d})")
    print('='*50)
    
    # Verify
    if elo1_after < elo1_before and elo2_after > elo2_before:
        print("✅ ELO được cập nhật đúng! (Người thua giảm, người thắng tăng)")
    else:
        print("⚠️ ELO có thể chưa được cập nhật đúng")

def run_elo_test():
    """
    Test UC ELO:
    1. Xem leaderboard
    2. Xem ELO history của user
    3. Xem stats của user
    """
    print("\n" + "="*60)
    print("              TEST UC ELO")
    print("="*60)
    
    # 1. Leaderboard
    print("\n[TEST 1] Lấy bảng xếp hạng ELO...")
    test_get_leaderboard(10)
    
    # 2. ELO History
    print("\n[TEST 2] Lấy lịch sử ELO của user 1...")
    test_get_elo_history(1)
    
    # 3. Stats
    print("\n[TEST 3] Lấy thống kê user 1...")
    test_get_stats(1)

def main():
    print("\n" + "#"*60)
    print("#     CHESS GAME - TEST SURRENDER & ELO")
    print("#"*60)
    
    import sys
    
    # Check for command line argument
    if len(sys.argv) > 1:
        choice = sys.argv[1]
    else:
        print("\nChọn test:")
        print("1. Test UC Surrender (Rời trận)")
        print("2. Test UC ELO (Leaderboard, History)")
        print("3. Chạy tất cả")
        print("0. Thoát")
        
        choice = input("\nNhập lựa chọn (0-3): ").strip()
    
    if choice == "1":
        run_surrender_test()
    elif choice == "2":
        run_elo_test()
    elif choice == "3":
        run_elo_test()
        run_surrender_test()
    elif choice == "0":
        print("Bye!")
    else:
        print("Lựa chọn không hợp lệ!")

if __name__ == "__main__":
    main()
