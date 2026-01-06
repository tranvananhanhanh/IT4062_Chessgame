# Hướng Dẫn Chia Sẻ Server Với Bạn

## 📡 Thông Tin Server Hiện Tại

**Server Status:** ✅ Đang chạy

| Thông Số | Giá Trị |
|---------|--------|
| **Host (Local)** | `localhost` |
| **Port (Local)** | `8888` |
| **Host (Local Network)** | `172.22.247.138` |
| **Port (Local Network)** | `8888` |

---

## 🔧 Cách 1: Dùng Ngrok Tunnel (Nên Dùng)

### Step 0: Tạo Ngrok Account & Lấy AuthToken
1. Đăng ký: https://dashboard.ngrok.com/signup
2. Vào: https://dashboard.ngrok.com/get-started/your-authtoken
3. Copy **AuthToken** (dòng dài, bắt đầu bằng `ngrok...`)
4. Install authtoken:
   ```bash
   ngrok config add-authtoken <YOUR_AUTHTOKEN>
   ```

### Step 1: Install Ngrok
```bash
# Đã cài sẵn bằng wget
which ngrok  # Kiểm tra
```

### Step 2: Start Ngrok Tunnel
```bash
ngrok tcp 8888
```

Output sẽ như:
```
Forwarding                    tcp://0.tcp.ap.ngrok.io:11527 -> localhost:8888
```

### Step 3: Gửi Cho Bạn
Từ output ngrok, bạn sẽ thấy:
```
HOST: 0.tcp.ap.ngrok.io (hoặc ngrok URL khác)
PORT: 11527 (hoặc port khác)
```

Client sẽ kết nối:
```bash
CHESS_SERVER_HOST=0.tcp.ap.ngrok.io CHESS_SERVER_PORT=11527 python3 main.py
```

---

## 🔧 Cách 2: Dùng Localhost (Nếu Cùng Mạng)

### Nếu bạn ở cùng wifi/mạng:

1. **Lấy local IP:**
   ```bash
   ipconfig getifaddr en0        # macOS
   hostname -I | awk '{print $1}' # Linux
   ```

2. **Gửi cho bạn:**
   ```
   HOST: [IP_VỪA_LẤY] (VD: 192.168.1.100)
   PORT: 8888
   ```

3. **Bạn chạy:**
   ```bash
   CHESS_SERVER_HOST=192.168.1.100 CHESS_SERVER_PORT=8888 python3 main.py
   ```

---

## ⚙️ Cách 3: Port Forward Router

Nếu bạn khác mạng, forward port router:

1. **Trên router settings:**
   - Port External: `15515` (hoặc tùy ý)
   - Port Internal: `8888`
   - Host: `[máy_bạn_local_ip]`

2. **Lấy public IP:**
   ```bash
   curl ifconfig.co
   ```

3. **Gửi cho bạn:**
   ```
   HOST: [PUBLIC_IP]
   PORT: 15515
   ```

---

## 🚀 Quick Test

### Test Local (cùng máy):
```bash
CHESS_SERVER_HOST=localhost CHESS_SERVER_PORT=8888 python3 main.py
```

### Test Ngrok (khác máy):
```bash
# Bạn ngrok tunnel:
ngrok tcp 8888

# Bạn bè chạy client:
CHESS_SERVER_HOST=0.tcp.ap.ngrok.io CHESS_SERVER_PORT=11527 python3 main.py
```

---

## 📝 Checklist Trước Khi Gửi

- [ ] Server đang chạy: `./bin/chess_server`
- [ ] Ngrok tunnel mở (nếu dùng cách 1)
- [ ] Firewall cho phép port 8888 (nếu cần)
- [ ] Database PostgreSQL đang chạy
- [ ] Bạn bè có đủ client files

---

## 🆘 Troubleshoot

**"Connection refused"**
- Kiểm tra server: `ps aux | grep chess_server`
- Kiểm tra port: `netstat -tlnp | grep 8888`

**"Ngrok đấu nối không ổn định"**
- Kiểm tra network
- Restart ngrok tunnel

**"Client không kết nối được"**
- Kiểm tra host/port đúng không
- Test local trước: `localhost:8888`
- Sau đó test ngrok

---

## 💡 Pro Tips

1. **Dùng screen để server chạy lâu:**
   ```bash
   screen -S chess_server
   ./bin/chess_server
   # Ctrl+A+D để minimize
   # screen -r chess_server để quay lại
   ```

2. **Dùng nohup để server chạy khi đóng terminal:**
   ```bash
   nohup ./bin/chess_server > chess_server.log 2>&1 &
   ```

3. **Xem logs:**
   ```bash
   tail -f chess_server.log
   ```
