# Tính Năng Chat Trong Trận PVP - Hướng Dẫn Tích Hợp

## Tổng Quan
Chức năng chat cho phép hai người chơi giao tiếp trực tiếp trong một trận đấu PVP. Pesan được gửi thông qua server và phân phối đến đối thủ.

## Tệp Đã Thêm/Sửa

### Server-Side (C)

#### 1. **include/protocol.h**
- Thêm 2 constants mới:
  - `CMD_GAME_CHAT` - Command để gửi chat trong trận
  - `RESP_GAME_CHAT_FROM` - Response khi nhận message từ đối thủ

#### 2. **include/game_chat.h** (TẠO MỚI)
```c
void handle_game_chat(ClientSession *session, int match_id, const char *message, PGconn *db);
```
- Header file cho game chat handler

#### 3. **src/game/game_chat.c** (TẠO MỚI)
- **Hàm chính**: `handle_game_chat()`
  - Nhận: session, match_id, message, database connection
  - Tìm đối thủ trong match từ database
  - Lấy socket fd của đối thủ từ bảng online_users
  - Gửi message với format: `GAME_CHAT_FROM|sender_username|message\n`
  
- **Hàm trợ**: `get_opponent_info()`
  - Query match từ DB để lấy ID của cả hai người chơi
  - Xác định đối thủ (nếu bạn là white, đối thủ là black, và ngược lại)
  - Lấy username của đối thủ từ DB

#### 4. **src/protocol/protocol_handler.c**
- Thêm `#include "game_chat.h"`
- Thêm routing cho command `GAME_CHAT`:
  ```c
  } else if (strcmp(command, "GAME_CHAT") == 0) {
      // GAME_CHAT|match_id|message
      int match_id = atoi(param1);
      handle_game_chat(session, match_id, param2, db);
  }
  ```

#### 5. **Makefile**
- Thêm `$(GAME_DIR)/game_chat.c` vào `GAME_SRCS`
- Thêm `@mkdir -p $(BUILD_DIR)/src/db`, `src/elo`, `src/login`, `src/matchmaking`, `src/chat` vào directories

---

### Client-Side (Python)

#### 1. **client/ui/game_chat_ui.py** (TẠO MỚI)
- **Class**: `GameChatUI(tk.Frame)`
- **UI Components**:
  - Header với tiêu đề "Match Chat"
  - ScrolledText widget (read-only) để hiển thị chat
  - Entry widget để nhập message
  - Send button
  
- **Phương thức chính**:
  - `add_message(sender_name, message, is_self=False)` - Thêm message vào display với timestamp
  - `send_message()` - Gửi message tới server qua `GAME_CHAT|match_id|message`
  - `add_system_message()` - Thêm system message (không có sender)
  - `clear_chat()` - Xóa tất cả messages

- **Features**:
  - Auto-scroll đến cuối khi có message mới
  - Enter key để gửi
  - Timestamp cho mỗi message
  - Color coding: đối thủ (đỏ), chính mình (xanh)

#### 2. **client/ui/pvp_ui.py**
- Thêm import: `from ui.game_chat_ui import GameChatUI`
- Sửa `setup_ui()`: 
  - Tạo 3 panels: left (board), center (chat), right (controls)
  - Khởi tạo `GameChatUI` với match_id và client
  
- Sửa `handle_message()`:
  - Thêm xử lý cho `GAME_CHAT_FROM`:
    ```python
    if msg.startswith("GAME_CHAT_FROM"):
        parts = msg.split('|', 2)
        if len(parts) >= 3:
            sender_name = parts[1]
            chat_message = parts[2]
            if self.game_chat and self.game_chat.winfo_exists():
                self.game_chat.add_message(sender_name, chat_message, is_self=False)
    ```

---

## Flow Của Chat Message

### Gửi Message (Client → Server → Client):
1. **Client A** gõ message vào input box
2. Click Send hoặc nhấn Enter
3. Client gửi: `GAME_CHAT|{match_id}|{message}\n`
4. **Server**:
   - Parse command → gọi `handle_game_chat()`
   - Tìm match_id trong DB → lấy user_id của đối thủ
   - Lấy socket fd của đối thủ từ `online_users`
   - Gửi: `GAME_CHAT_FROM|{sender_username}|{message}\n` tới client B
5. **Client B**:
   - `poll_server()` nhận message
   - Forward tới `PvPUI.handle_message()`
   - Hiển thị trong chat box

---

## Cách Sử Dụng

### Từ Client:
```python
# Chat message được tự động gửi khi click Send button
# Message format sent to server: GAME_CHAT|match_id|message
client.send(f"GAME_CHAT|{self.match_id}|Hello opponent!")
```

### Từ Server:
```c
// Server nhận: GAME_CHAT|123|Hello opponent!
// Server gửi cho đối thủ: GAME_CHAT_FROM|player1|Hello opponent!
```

---

## Protocol Specification

### Client → Server:
```
GAME_CHAT|{match_id}|{message}
```
- `match_id`: ID của trận đấu hiện tại
- `message`: Nội dung chat message

### Server → Client:
```
GAME_CHAT_FROM|{sender_username}|{message}
```
- `sender_username`: Tên người chơi gửi message
- `message`: Nội dung chat

---

## Lưu Ý Kỹ Thuật

1. **Thread Safety**: 
   - Server sử dụng `online_users` mutex khi truy cập socket
   - Client forward message qua queue nếu listener có `message_queue`

2. **Error Handling**:
   - Nếu đối thủ không online, message bị ignore (không báo lỗi)
   - Nếu send() thất bại, in warning nhưng không crash

3. **UI Integration**:
   - Chat UI được tích hợp giữa board (left) và game controls (right)
   - Layout: [Board] [Chat] [Controls]

4. **Database Queries**:
   - Lookup match_id để lấy white_user_id, black_user_id
   - Lookup user_id để lấy username

---

## Testing

### Để test chức năng chat:
1. Build server: `cd server-c && make`
2. Run server: `./bin/chess_server`
3. Start 2 client: `python3 client/main.py` (x2)
4. Cả hai đăng nhập và tìm trận (matchmaking)
5. Khi trận bắt đầu, test gửi message từ cả hai phía
6. Verify messages xuất hiện trong chat box của đối thủ

---

## Mở Rộng Tương Lai

Có thể thêm:
- Lưu chat history vào database
- Emoji/special characters support
- Chat notifications
- Mute/block chat feature
- Chat reactions
- File/image sharing
