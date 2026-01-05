## FIX CHAT ISSUE - HƯỚNG DẪN TEST

### 🔧 Vấn Đề Đã Fix:
1. **Column name sai**: Server tìm `username` nhưng database dùng `name`
   - Fixed: Đổi tất cả query từ `SELECT username` → `SELECT name`
2. **Debug logging**: Thêm logs chi tiết để theo dõi chat flow

### ✅ Rebuild & Test

#### 1. Server đã được rebuild
```bash
cd server-c
make clean && make
./bin/chess_server
```

Server sẽ chạy và log chi tiết:
```
[GameChat] User 5 sending message to match 123: Hello!
[GameChat] Opponent ID: 6, FD: 8, Name: opponent_username
[GameChat] Sending to opponent (fd=8): GAME_CHAT_FROM|player1|Hello!
```

#### 2. Client Config
Đã set port `11527` theo bạn bạn mở:
```python
# poll_client.py
port = int(port or os.getenv("CHESS_SERVER_PORT", 11527))
```

#### 3. Test Chat Workflow
1. **Người A** (bạn) login → khởi tạo PvP match
2. **Người B** (bạn bè) login → join match
3. Khi trận bắt đầu → chat box xuất hiện
4. A gửi message → B sẽ nhận ngay
5. B gửi message → A sẽ nhận ngay

### 🐛 Debug Checklist

Nếu vẫn không nhận tin nhắn:

1. **Kiểm tra server logs:**
   ```bash
   tail -f /tmp/chess_server.log
   ```
   Xem có `[GameChat]` messages không?

2. **Kiểm tra client logs:**
   - Client sẽ print `[DEBUG PollClient] Message: GAME_CHAT_FROM|...`
   - Nếu không thấy, message không được gửi từ server

3. **Kiểm tra match tồn tại:**
   - Đảm bảo `match_id` hiện tại đúng
   - Server phải tìm được white_user_id và black_user_id

4. **Kiểm tra online_users:**
   - Khi login, user phải được thêm vào online_users table
   - Server phải tìm được socket_fd của đối thủ

### 📝 Key Changes Made

**File: src/game/game_chat.c**
```c
// BEFORE (WRONG):
"SELECT username FROM users WHERE user_id = %d"

// AFTER (CORRECT):
"SELECT name FROM users WHERE user_id = %d"
```

**Added debug logging:**
```c
printf("[GameChat] User %d sending message to match %d: %s\n", ...);
printf("[GameChat] Opponent ID: %d, FD: %d, Name: %s\n", ...);
printf("[GameChat] Sending to opponent (fd=%d): %s\n", ...);
```

### 🎯 Expected Behavior Now

**Server Console:**
```
[GameChat] User 1 sending message to match 100: Test message
[GameChat] Opponent ID: 2, FD: 9, Name: player2
[GameChat] Sending to opponent (fd=9): GAME_CHAT_FROM|player1|Test message
```

**Client 1 Console:**
```
[SENT] GAME_CHAT|100|Test message
```

**Client 2 Console:**
```
[DEBUG PollClient] Message: GAME_CHAT_FROM|player1|Test message
```

**Client 2 UI:**
```
Chat box sẽ hiển thị:
[HH:MM] player1: Test message
```

### 🚀 Deploy

1. Server được rebuild lại, sẵn sàng chạy
2. Client không cần thay đổi (chỉ cần server fix)
3. Test ngay với bạn bè!

### ✨ Notes

- Chat không lưu vào database (temporary only)
- Message được gửi real-time khi cả 2 online
- Nếu đối thủ offline, message bị ignore (không báo lỗi)
