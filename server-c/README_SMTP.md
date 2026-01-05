# Hướng dẫn cấu hình SMTP cho Chess Game Server

Server giờ tự động đọc file `.env` khi khởi động. Bạn chỉ cần:

## Bước 1: Tạo file .env

```bash
cd server-c
cp .env.example .env
```

## Bước 2: Lấy App Password từ Gmail

1. Truy cập: https://myaccount.google.com/apppasswords
2. Đăng nhập Gmail (cần bật 2FA trước)
3. Chọn app: "Mail", device: "Other" → đặt tên "Chess Server"
4. Click "Generate" → Copy mã 16 ký tự (bỏ dấu cách)

## Bước 3: Sửa file .env

Mở `server-c/.env` và điền thông tin:

```bash
SMTP_HOST=smtp.gmail.com
SMTP_PORT=587
SMTP_USE_TLS=true
SMTP_USER=yourgmail@gmail.com      # ← Gmail của bạn
SMTP_PASS=abcdabcdabcdabcd          # ← App Password 16 ký tự
SMTP_FROM=yourgmail@gmail.com      # ← Gmail của bạn
```

## Bước 4: Chạy server

```bash
cd server-c
./bin/chess_server
```

Server sẽ tự động load `.env` và gửi OTP thực qua Gmail!

---

**Lưu ý:**
- Không commit file `.env` vào git (chứa mật khẩu)
- Nếu thiếu `.env`, server sẽ in OTP ra console (fallback)
- Phải dùng App Password, không dùng mật khẩu Gmail thường
