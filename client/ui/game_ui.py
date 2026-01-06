import tkinter as tk
from tkinter import messagebox
from network.poll_client import PollClient
from ui.gamebot_ui import GameBotUI
from ui.pvp_ui import PvPUI
from ui.friend_ui import FriendUI
from ui.login_ui import LoginUI, RegisterUI
from ui.login_ui import LoginUI, RegisterUI, ForgotPasswordUI
from ui.history_ui import HistoryUI
from ui.leaderboard_ui import LeaderboardUI
from ui.notifications import show_toast
import sys
import queue  # ← THÊM: Cho thread-safe message forwarding

FONT_TITLE = ("Helvetica", 18, "bold")
FONT_LABEL = ("Helvetica", 11)
FONT_BUTTON = ("Helvetica", 11, "bold")

class ChessApp:
    def __init__(self, master):
        self.master = master

        # ===== ROOT FULLSCREEN =====
        try:
            if sys.platform == "darwin":  # macOS
                self.master.attributes("-fullscreen", True)
            elif sys.platform == "win32":  # Windows
                self.master.state("zoomed")
            else:  # Linux
                self.master.attributes('-zoomed', True)
        except Exception as e:
            # Fallback: just maximize normally
            print(f"Could not maximize window: {e}")
            try:
                self.master.geometry("1200x800")
            except:
                pass

        self.master.configure(bg="#f8f5ff")

        self.client = PollClient()
        self.username = None
        self.user_id = None
        self.elo = 1200  # Default ELO

        self.current_frame = None
        self.login_ui = None
        self.register_ui = None
        self.pending_action = None
        self.pending_context = {}
        self.listeners = []
        self.poll_server()
        self.login_frame()

    # ===== Utilities =====
    def clear(self):
        # Cleanup PvP UI if it exists
        if hasattr(self, 'pvp_ui') and self.pvp_ui:
            self.pvp_ui.destroy()
            self.pvp_ui = None
        if self.current_frame:
            self.current_frame.destroy()
        if self.login_ui:
            self.login_ui.destroy()
            self.login_ui = None
        if self.register_ui:
            self.register_ui.destroy()
            self.register_ui = None
        # Clear status message
        if hasattr(self, 'status_label'):
            self.status_label.destroy()
            delattr(self, 'status_label')

    def create_button(self, parent, text, command):
        return tk.Button(
            parent,
            text=text,
            font=FONT_BUTTON,
            width=32,
            height=2,
            pady=3,
            padx=10,
            command=command,
            relief=tk.FLAT,
            bg="#3498db",
            fg="white",
            activebackground="#2980b9",
            activeforeground="white",
            cursor="hand2",
            bd=0
        )

    def show_status(self, msg):
        if not hasattr(self, 'status_label'):
            self.status_label = tk.Label(self.master, text="", fg="red", font=FONT_LABEL)
            self.status_label.pack(side="bottom", fill="x")
        self.status_label.config(text=msg)

    def add_listener(self, listener):
        if listener not in self.listeners:
            self.listeners.append(listener)

    def remove_listener(self, listener):
        if listener in self.listeners:
            self.listeners.remove(listener)

    # ===== Login / Register =====
    def login_frame(self):
        self.clear()
        self.login_ui = LoginUI(self.master, self.login, self.register)
        self.login_ui.set_switch_callback(self.register_frame)
        self.login_ui.set_forgot_callback(self.forgot_password_frame)
        self.current_frame = self.login_ui.frame
    
    def register_frame(self):
        self.clear()
        self.register_ui = RegisterUI(self.master, self.register, self.login_frame)
        self.current_frame = self.register_ui.frame

    def forgot_password_frame(self):
        self.clear()
        self.forgot_ui = ForgotPasswordUI(
            self.master,
            self.request_otp,
            self.reset_password,
            self.login_frame
        )
        self.current_frame = self.forgot_ui.frame

    def login(self, username, password):
        self.client.send(f"LOGIN|{username}|{password}\n")
        self.pending_action = "login"
        self.pending_context = {"username": username}

    def register(self, username, password, email):
        self.client.send(f"REGISTER|{username}|{password}|{email}\n")
        self.pending_action = "register"
    # ===== Forgot Password =====
    def request_otp(self, email):
        self.client.send(f"FORGOT_PASSWORD|{email}\n")
        self.pending_action = "forgot"
        self.pending_context = {"email": email}

    def reset_password(self, user_id, otp, new_password):
        self.client.send(f"RESET_PASSWORD|{user_id}|{otp}|{new_password}\n")
        self.pending_action = "reset_password"

    # ===== Main Menu =====
    def main_menu(self):
        self.clear()
        frame = tk.Frame(self.master, bg="#ecf0f1")
        frame.pack(expand=True, fill="both")
        self.current_frame = frame

        # Centered menu container with fixed size
        container = tk.Frame(frame, bg="#ffffff", relief=tk.RAISED, bd=1, width=580, height=520)
        container.pack(expand=True, padx=20, pady=20)
        container.pack_propagate(False)  # Prevent resizing
        
        # Inner padding
        inner = tk.Frame(container, bg="#ffffff", padx=18, pady=18)
        inner.pack(fill="both", expand=True)

        # Main title
        tk.Label(inner, text="MENU CHÍNH", font=("Helvetica", 20, "bold"), 
                 bg="#ffffff", fg="#2c3e50").pack(pady=(0, 12))
        
        # User info box
        info_box = tk.Frame(inner, bg="#ecf0f1", padx=10, pady=8, relief=tk.FLAT)
        info_box.pack(fill="x", pady=(0, 12))
        
        tk.Label(info_box, text=f"Xin chào, {self.username}", font=("Helvetica", 11, "bold"), 
                 bg="#ecf0f1", fg="#2c3e50").pack(anchor="w")
        tk.Label(info_box, text=f"Mã số: {self.user_id}", font=("Helvetica", 9), 
                 bg="#ecf0f1", fg="#7f8c8d").pack(anchor="w", pady=(1, 0))
        tk.Label(info_box, text=f"♔ Điểm ELO: {self.elo}", font=("Helvetica", 10, "bold"), 
                 bg="#ecf0f1", fg="#27ae60").pack(anchor="w", pady=(1, 0))

        # Buttons container
        buttons_frame = tk.Frame(inner, bg="#ffffff")
        buttons_frame.pack(fill="both", expand=True)

        self.create_button(buttons_frame, "⚔ Tìm Trận", self.start_matchmaking).pack(pady=3, fill="x")
        self.create_button(buttons_frame, "♞ Kết Bạn", self.friend_request_frame).pack(pady=3, fill="x")
        self.create_button(buttons_frame, "♚ Chơi Với Bot", self.start_bot_game).pack(pady=3, fill="x")
        self.create_button(buttons_frame, "♜ Xem Lịch Sử", self.show_history).pack(pady=3, fill="x")
        self.create_button(buttons_frame, "♛ Bảng Xếp Hạng", self.show_leaderboard).pack(pady=3, fill="x")
        
        # Logout button with different color
        logout_btn = tk.Button(
            buttons_frame,
            text="↪ Đăng Xuất",
            font=FONT_BUTTON,
            width=32,
            height=2,
            pady=3,
            padx=10,
            command=self.logout,
            relief=tk.FLAT,
            bg="#e74c3c",
            fg="white",
            activebackground="#c0392b",
            activeforeground="white",
            cursor="hand2",
            bd=0
        )
        logout_btn.pack(pady=(8, 0), fill="x")

    # ===== Friend Request =====
    def friend_request_frame(self):
        self.clear()
        self.friend_ui = FriendUI(self.master, self.user_id, self.main_menu, self.client)
        self.add_listener(self.friend_ui)

    # ===== Network actions =====
    def send_friend_request(self):
        friend_id = self.friend_id_entry.get()
        self.client.send(f"FRIEND_REQUEST|{self.user_id}|{friend_id}\n")
        self.pending_action = "friend_request"

    def send_game_control(self, action):
        self.client.send(f"{action}|{self.user_id}\n")
        self.pending_action = "game_control"

    def logout(self):
        self.client.send(f"LOGOUT|{self.user_id}\n")
        self.pending_action = "logout"

    # ← SỬA: poll_server() – Forward qua queue nếu listener có message_queue (thread-safe)
    def poll_server(self):
        try:
            responses = self.client.poll()
        except ConnectionError as e:
            print("[Client] Server disconnected:", e)
            return  # ⛔ DỪNG POLLING NGAY

        for resp in responses:
            self.handle_server_message(resp)
            # Forward đến listeners: Nếu listener có queue, push vào queue (thread-safe)
            for listener in self.listeners:
                if hasattr(listener, 'message_queue'):
                    listener.message_queue.put(resp)  # ← PUSH VÀO QUEUE CỦA UI (fix integrate!)
                elif hasattr(listener, 'handle_message'):
                    listener.handle_message(resp)  # Fallback nếu không có queue

        self.master.after(30, self.poll_server)

    # ← SỬA: handle_server_message – Thêm xử lý BOT_MOVE_RESULT (forward đến UI nếu đang bot game)
    def handle_server_message(self, resp):
        resp = resp.strip()

        # Handle generic ERROR messages first
        if resp.startswith("ERROR|") and not self.pending_action:
            print(f"[Error from server] {resp}")
        
        if self.pending_action == "login":
            if resp.startswith("LOGIN_SUCCESS"):
                parts = resp.split('|')
                self.username = self.pending_context.get("username")
                self.user_id = int(parts[1])
                # Parse ELO if available (format: LOGIN_SUCCESS|user_id|name|elo)
                if len(parts) >= 4:
                    try:
                        self.elo = int(parts[3])
                    except ValueError:
                        self.elo = 1200  # Default if parsing fails
                # Toast success
                show_toast(self.master, "Đăng nhập thành công", kind="success")
                self.main_menu()
            elif resp.startswith("LOGIN_FAIL") or resp.startswith("ERROR"):
                # Show error toast
                show_toast(self.master, "Thông tin tài khoản hoặc mật khẩu không chính xác", kind="error")
            self.pending_action = None
            self.pending_context = {}
        elif self.pending_action == "register":
            if resp.startswith("REGISTER_OK"):
                # Hiển thông báo thành công rồi chuyển về login
                if hasattr(self, 'register_ui') and self.register_ui:
                    self.register_ui.show_success_and_redirect()
                else:
                    show_toast(self.master, "Đăng ký thành công! Vui lòng đăng nhập.", kind="success")
                    self.login_frame()
            else:
                print("Register failed: " + resp)
            self.pending_action = None
            self.pending_context = {}
        elif self.pending_action == "forgot":
            if resp.startswith("OTP_SENT"):
                parts = resp.split('|')
                if len(parts) >= 3 and hasattr(self, 'forgot_ui') and self.forgot_ui:
                    user_id = int(parts[1])
                    email = parts[2]
                    self.forgot_ui.show_step2(user_id, email)
                else:
                    show_toast(self.master, "Đã gửi OTP", kind="success")
            else:
                show_toast(self.master, "Gửi OTP thất bại", kind="error")
            self.pending_action = None
            self.pending_context = {}
        elif self.pending_action == "reset_password":
            if resp.startswith("PASSWORD_RESET_OK"):
                if hasattr(self, 'forgot_ui') and self.forgot_ui:
                    self.forgot_ui.show_success_and_redirect()
                else:
                    show_toast(self.master, "Đặt lại mật khẩu thành công! Vui lòng đăng nhập.", kind="success")
                    self.login_frame()
            else:
                show_toast(self.master, "Đặt lại mật khẩu thất bại", kind="error")
            self.pending_action = None
            self.pending_context = {}
        elif self.pending_action == "friend_request":
            if resp.startswith("FRIEND_REQUESTED"):
                show_toast(self.master, "Gửi lời mời kết bạn thành công", kind="success")
            else:
                show_toast(self.master, "Gửi lời mời kết bạn thất bại", kind="error")
            self.pending_action = None
            self.pending_context = {}
        elif self.pending_action == "game_control":
            print(resp)
            self.pending_action = None
            self.pending_context = {}
        elif self.pending_action == "logout":
            self.username = None
            self.user_id = None
            self.login_frame()
            self.pending_action = None
            self.pending_context = {}
        elif self.pending_action == "matchmaking":
            if resp.startswith("MATCH_FOUND"):
                parts = resp.split('|')
                match_id = parts[1]
                opponent_name = parts[2] if len(parts) > 2 else "Unknown"
                color = parts[3] if len(parts) > 3 else "white"
                print(f"Đã tìm thấy trận đấu! Đối thủ: {opponent_name}, Bạn chơi quân: {color}")
                self.pending_action = None
                # Start PvP game UI
                self.start_pvp_game(match_id, color, opponent_name)
            elif resp.startswith("MATCHMAKING_LEFT"):
                print("Đã hủy tìm trận")
                self.pending_action = None
                self.main_menu()
            elif resp.startswith("MATCHMAKING_QUEUED"):
                # Still waiting
                if hasattr(self, 'matchmaking_status'):
                    self.matchmaking_status.config(text="Đang tìm đối thủ...")
            elif resp.startswith("ERROR"):
                print("Lỗi matchmaking: " + resp)
                self.pending_action = None

        # ← THÊM: Xử lý BOT_MOVE_RESULT (nếu từ server, forward đến bot UI nếu đang chơi)
        elif resp.startswith("BOT_MOVE_RESULT|"):
            # Nếu đang chơi bot và có gamebot_ui, forward trực tiếp (vì queue đã handle)
            if hasattr(self, 'gamebot_ui') and self.gamebot_ui:
                print(f"[App DEBUG] Forwarding BOT_MOVE_RESULT to GameBotUI: {resp}")
                # Push vào queue của UI (đã add_listener ở start_bot_game)
                if hasattr(self.gamebot_ui, 'message_queue'):
                    self.gamebot_ui.message_queue.put(resp)
                else:
                    self.gamebot_ui.handle_server_message(resp)  # Fallback

        # ← GIỮ NGUYÊN: Xử lý BOT_MATCH_CREATED (tạo UI sau khi nhận từ server)
        elif resp.startswith("BOT_MATCH_CREATED|"):
            self.bot_starting = False

            _, match_id, fen = resp.split('|', 2)

            self.clear()
            self.gamebot_ui = GameBotUI(
                self.master,
                int(match_id),
                self.client,
                self.main_menu,
                difficulty=self.selected_bot_difficulty
            )
            self.add_listener(self.gamebot_ui)
            self.gamebot_ui.frame.pack(fill="both", expand=True)
            self.current_frame = self.gamebot_ui.frame
            self.master.update_idletasks()

    def start_bot_game(self):
        def on_select_difficulty(difficulty):
            difficulty_window.destroy()

            self.selected_bot_difficulty = difficulty
            self.bot_starting = True

            # CHỈ SEND – KHÔNG POLL
            self.client.send(f"MODE_BOT|{self.user_id}|{difficulty}\n")

        # Create dialog window
        difficulty_window = tk.Toplevel(self.master)
        difficulty_window.title("Chọn Độ Khó Bot")
        difficulty_window.geometry("400x400")
        difficulty_window.resizable(False, False)
        difficulty_window.configure(bg="#f0f0f0")
        
        # Center window
        difficulty_window.transient(self.master)
        difficulty_window.grab_set()
        
        # Title
        title_frame = tk.Frame(difficulty_window, bg="#2c3e50", height=60)
        title_frame.pack(fill="x")
        tk.Label(
            title_frame,
            text="♟ Chọn Độ Khó",
            font=("Helvetica", 16, "bold"),
            bg="#2c3e50",
            fg="white"
        ).pack(pady=15)
        
        # Content frame
        content = tk.Frame(difficulty_window, bg="#f0f0f0", padx=30, pady=20)
        content.pack(fill="both", expand=True)
        
        tk.Label(
            content,
            text="Chọn mức độ khó của đối thủ máy:",
            font=("Helvetica", 11),
            bg="#f0f0f0",
            fg="#333"
        ).pack(pady=(0, 20))
        
        # Easy button
        easy_btn = tk.Button(
            content,
            text=" Dễ ",
            font=("Helvetica", 12, "bold"),
            bg="#4CAF50",
            fg="white",
            activebackground="#45a049",
            activeforeground="white",
            width=25,
            height=2,
            cursor="hand2",
            command=lambda: on_select_difficulty("easy")
        )
        easy_btn.pack(pady=8)
        
        # Hard button
        hard_btn = tk.Button(
            content,
            text=" Khó ",
            font=("Helvetica", 12, "bold"),
            bg="#f44336",
            fg="white",
            activebackground="#da190b",
            activeforeground="white",
            width=25,
            height=2,
            cursor="hand2",
            command=lambda: on_select_difficulty("hard")
        )
        hard_btn.pack(pady=8)
        
        # Cancel button
        cancel_btn = tk.Button(
            content,
            text="Hủy",
            font=("Helvetica", 10),
            bg="#95a5a6",
            fg="white",
            activebackground="#7f8c8d",
            activeforeground="white",
            width=15,
            cursor="hand2",
            command=difficulty_window.destroy
        )
        cancel_btn.pack(pady=(15, 0))

    def _back_to_menu_and_resize(self):
        self.master.geometry("600x500")
        self.main_menu()

    # ===== Matchmaking =====
    def start_matchmaking(self):
        self.client.send(f"JOIN_MATCHMAKING|{self.user_id}\n")
        self.pending_action = "matchmaking"
        
        # Show waiting screen
        self.clear()
        frame = tk.Frame(self.master, padx=20, pady=20)
        frame.pack(expand=True)
        self.current_frame = frame
        
        tk.Label(frame, text="Đang tìm đối thủ...", font=FONT_TITLE).pack(pady=(0, 20))
        self.matchmaking_status = tk.Label(frame, text="Vui lòng đợi", font=FONT_LABEL)
        self.matchmaking_status.pack(pady=10)
        
        cancel_btn = self.create_button(frame, "Hủy", self.cancel_matchmaking)
        cancel_btn.pack(pady=20)
    
    def cancel_matchmaking(self):
        self.client.send(f"LEAVE_MATCHMAKING|{self.user_id}\n")
        self.pending_action = None
        self.main_menu()
    
    def start_pvp_game(self, match_id, my_color, opponent_name):
        """Start PvP game UI"""
        self.clear()
        try:
            # Maximize window for game
            if sys.platform == "darwin":
                self.master.attributes("-fullscreen", True)
            elif sys.platform == "win32":
                self.master.state("zoomed")
            else:
                self.master.attributes('-zoomed', True)
        except:
            self.master.geometry("1200x800")
        
        self.pvp_ui = PvPUI(self.master, match_id, my_color, opponent_name, 
                           self.client, self.main_menu, self.user_id)
        self.current_frame = self.pvp_ui.frame
        self.add_listener(self.pvp_ui)
        self.master.update_idletasks()
    
    def show_history(self):
        """Show game history UI"""
        self.clear()
        try:
            # Maximize window for history view
            if sys.platform == "darwin":
                self.master.attributes("-fullscreen", True)
            elif sys.platform == "win32":
                self.master.state("zoomed")
            else:
                self.master.attributes('-zoomed', True)
        except:
            self.master.geometry("1200x800")
        
        # Pass client to HistoryUI so it can use socket connection
        # HistoryUI(master, user_id, client, on_back)
        self.history_ui = HistoryUI(self.master, self.user_id, self.client, self._back_from_history)
        self.current_frame = self.history_ui.frame
        self.add_listener(self.history_ui)
        self.master.update_idletasks()

    def _back_from_history(self):
        if hasattr(self, 'history_ui') and self.history_ui:
            self.remove_listener(self.history_ui)
        self.main_menu()

    def show_leaderboard(self):
        """Show leaderboard UI"""
        self.clear()
        try:
            if sys.platform == "darwin":
                self.master.attributes("-fullscreen", True)
            elif sys.platform == "win32":
                self.master.state("zoomed")
            else:
                self.master.attributes('-zoomed', True)
        except:
            self.master.geometry("1200x800")

        self.leaderboard_ui = LeaderboardUI(self.master, self.client, self._back_from_leaderboard)
        self.current_frame = self.leaderboard_ui.frame
        self.add_listener(self.leaderboard_ui)
        self.leaderboard_ui.refresh()
        self.master.update_idletasks()

    def _back_from_leaderboard(self):
        if hasattr(self, 'leaderboard_ui') and self.leaderboard_ui:
            self.remove_listener(self.leaderboard_ui)
        self.main_menu()