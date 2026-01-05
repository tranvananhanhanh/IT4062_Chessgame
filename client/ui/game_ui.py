import tkinter as tk
from tkinter import messagebox
from network.poll_client import PollClient
from ui.gamebot_ui import GameBotUI
from ui.pvp_ui import PvPUI
from ui.friend_ui import FriendUI
from ui.login_ui import LoginUI
from ui.history_ui import HistoryUI
from ui.leaderboard_ui import LeaderboardUI
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

        self.current_frame = None
        self.login_ui = None
        self.pending_action = None
        self.pending_context = {}
        self.listeners = []
        self.poll_server()
        self.login_frame()

    # ===== Utilities =====
    def clear(self):
        if self.current_frame:
            self.current_frame.destroy()
        if self.login_ui:
            self.login_ui.destroy()
            self.login_ui = None

    def create_button(self, parent, text, command):
        return tk.Button(
            parent,
            text=text,
            font=FONT_BUTTON,
            width=20,
            pady=5,
            command=command
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
        self.current_frame = self.login_ui.frame

    def login(self, username, password):
        self.client.send(f"LOGIN|{username}|{password}\n")
        self.pending_action = "login"
        self.pending_context = {"username": username}
        self.show_status("")

    def register(self, username, password, email):
        self.client.send(f"REGISTER|{username}|{password}|{email}\n")
        self.pending_action = "register"
        self.show_status("")

    # ===== Main Menu =====
    def main_menu(self):
        self.clear()
        frame = tk.Frame(self.master, padx=20, pady=20)
        frame.pack(expand=True)
        self.current_frame = frame

        tk.Label(frame, text=f"Welcome, {self.username}", font=FONT_TITLE).pack(pady=(0, 10))
        tk.Label(frame, text=f"Your ID: {self.user_id}", font=FONT_LABEL, fg="#666").pack(pady=(0, 20))

        self.create_button(frame, "Tìm trận (Matchmaking)", self.start_matchmaking).pack(pady=5)
        self.create_button(frame, "Kết bạn", self.friend_request_frame).pack(pady=5)
        self.create_button(frame, "Chơi với Bot", self.start_bot_game).pack(pady=5)
        self.create_button(frame, "Xem lịch sử (History)", self.show_history).pack(pady=5)
        self.create_button(frame, "Bảng xếp hạng", self.show_leaderboard).pack(pady=5)
        self.create_button(frame, "Logout", self.logout).pack(pady=(20, 0))

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
        self.show_status("")

    def send_game_control(self, action):
        self.client.send(f"{action}|{self.user_id}\n")
        self.pending_action = "game_control"
        self.show_status("")

    def logout(self):
        self.client.send(f"LOGOUT|{self.user_id}\n")
        self.pending_action = "logout"
        self.show_status("")

    # ← SỬA: poll_server() – Forward qua queue nếu listener có message_queue (thread-safe)
    def poll_server(self):
        try:
            responses = self.client.poll()
        except ConnectionError as e:
            print("[Client] Server disconnected:", e)
            self.show_status("Mất kết nối tới server")
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
            self.show_status(resp)
        
        if self.pending_action == "login":
            if resp.startswith("LOGIN_SUCCESS"):
                self.username = self.pending_context.get("username")
                self.user_id = int(resp.split('|')[1])
                self.main_menu()
            elif resp.startswith("LOGIN_FAIL") or resp.startswith("ERROR"):
                self.show_status("Login failed: " + resp)
            self.pending_action = None
            self.pending_context = {}
        elif self.pending_action == "register":
            if resp.startswith("REGISTER_OK"):
                self.show_status("Đăng ký thành công!")
            else:
                self.show_status("Register failed: " + resp)
            self.pending_action = None
            self.pending_context = {}
        elif self.pending_action == "friend_request":
            if resp.startswith("FRIEND_REQUESTED"):
                self.show_status("Đã gửi lời mời kết bạn!")
            else:
                self.show_status("Kết bạn thất bại: " + resp)
            self.pending_action = None
            self.pending_context = {}
        elif self.pending_action == "game_control":
            self.show_status(resp)
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
                self.show_status(f"Đã tìm thấy trận đấu! Đối thủ: {opponent_name}, Bạn chơi quân: {color}")
                self.pending_action = None
                # Start PvP game UI
                self.start_pvp_game(match_id, color, opponent_name)
            elif resp.startswith("MATCHMAKING_LEFT"):
                self.show_status("Đã hủy tìm trận")
                self.pending_action = None
                self.main_menu()
            elif resp.startswith("MATCHMAKING_QUEUED"):
                # Still waiting
                if hasattr(self, 'matchmaking_status'):
                    self.matchmaking_status.config(text="Đang tìm đối thủ...")
            elif resp.startswith("ERROR"):
                self.show_status("Lỗi matchmaking: " + resp)
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

        difficulty_window = tk.Toplevel(self.master)
        difficulty_window.title("Chọn độ khó")

        tk.Button(
            difficulty_window,
            text="Easy",
            command=lambda: on_select_difficulty("easy")
        ).pack(padx=20, pady=10)

        tk.Button(
            difficulty_window,
            text="Hard",
            command=lambda: on_select_difficulty("hard")
        ).pack(padx=20, pady=10)

    def _back_to_menu_and_resize(self):
        self.master.geometry("400x350")
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
        self.history_ui = HistoryUI(self.master, self.user_id, self.client, self.main_menu)
        self.current_frame = self.history_ui.frame
        self.master.update_idletasks()

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
