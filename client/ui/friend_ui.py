import tkinter as tk
from tkinter import ttk

class FriendUI:
    def __init__(self, master, user_id, on_back, client):
        self.master = master
        self.client = client
        self.user_id = user_id
        self.on_back = on_back

        # ===== Always fullscreen for FriendUI =====
        import sys
        if sys.platform == "darwin":
            self.master.attributes("-fullscreen", True)
        else:
            self.master.state("zoomed")
        # ❌ KHÔNG dùng geometry()
        # self.master.geometry("920x580")
        self.master.configure(bg="#fdf2f8")

        # ===== Frame gốc không padding =====
        self.frame = tk.Frame(master, bg="#fdf2f8")
        self.frame.pack(fill="both", expand=True)

        # ===== Container có padding =====
        container = tk.Frame(self.frame, bg="#fdf2f8", padx=24, pady=24)
        container.pack(fill="both", expand=True)

        # ===== Title =====
        tk.Label(
            container,
            text="♟️ CHESS FRIEND CENTER ♟️",
            font=("Helvetica", 22, "bold"),
            bg="#fdf2f8",
            fg="#1f2937"
        ).pack(pady=(0, 18))

        # ===== Main content =====
        content = tk.Frame(container, bg="#fdf2f8")
        content.pack(fill="both", expand=True)

        content.columnconfigure(0, weight=1)
        content.columnconfigure(1, weight=2)
        content.rowconfigure(0, weight=1)

        # ================= LEFT: FRIEND LIST =================
        left = tk.LabelFrame(
            content,
            text="♞ Danh sách bạn bè",
            font=("Helvetica", 14, "bold"),
            bg="#eef6ff",
            fg="#1f2937",
            padx=10,
            pady=10
        )
        left.grid(row=0, column=0, sticky="nsew", padx=(0, 12))

        self.friends_listbox = tk.Listbox(
            left,
            font=("Helvetica", 13),
            bg="#ffffff",
            fg="#1f2937",
            selectbackground="#93c5fd",
            height=16
        )
        self.friends_listbox.pack(side="left", fill="both", expand=True)

        friend_scroll = ttk.Scrollbar(left, orient="vertical", command=self.friends_listbox.yview)
        friend_scroll.pack(side="right", fill="y")
        self.friends_listbox.config(yscrollcommand=friend_scroll.set)

        # ================= RIGHT: FRIEND REQUESTS =================
        right = tk.LabelFrame(
            content,
            text="♜ Lời mời kết bạn",
            font=("Helvetica", 14, "bold"),
            bg="#eef6ff",
            fg="#1f2937",
            padx=10,
            pady=10
        )
        right.grid(row=0, column=1, sticky="nsew")

        canvas = tk.Canvas(
            right,
            bg="#ffffff",
            highlightthickness=1,
            highlightbackground="#93c5fd"
        )
        canvas.pack(side="left", fill="both", expand=True)

        scrollbar = ttk.Scrollbar(right, orient="vertical", command=canvas.yview)
        scrollbar.pack(side="right", fill="y")

        canvas.configure(yscrollcommand=scrollbar.set)

        self.requests_container = tk.Frame(canvas, bg="#ffffff")
        canvas.create_window((0, 0), window=self.requests_container, anchor="nw")

        self.requests_container.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )

        # ================= ADD FRIEND =================
        add_frame = tk.Frame(container, bg="#fdf2f8")
        add_frame.pack(fill="x", pady=12)

        tk.Label(
            add_frame,
            text="♕ ID người chơi:",
            font=("Helvetica", 12, "bold"),
            bg="#fdf2f8",
            fg="#1f2937"
        ).pack(side="left")

        self.search_entry = ttk.Entry(add_frame, width=14)
        self.search_entry.pack(side="left", padx=6)

        ttk.Button(add_frame, text="➕ Kết bạn", command=self.send_friend_request).pack(side="left", padx=5)

        # ================= BOTTOM =================
        bottom = tk.Frame(container, bg="#fdf2f8")
        bottom.pack(fill="x", pady=(10, 0))

        ttk.Button(bottom, text="🔄 Làm mới", command=self.refresh).pack(side="left")
        ttk.Button(bottom, text="⬅ Quay lại", command=self.back).pack(side="right")

        self.status_label = tk.Label(
            container,
            text="",
            fg="#be185d",
            bg="#fdf2f8",
            font=("Helvetica", 11, "italic")
        )
        self.status_label.pack(fill="x", pady=(6, 0))

        self.refresh()

    # ================= NETWORK =================

    def refresh(self):
        self.client.send(f"FRIEND_LIST|{self.user_id}")
        self.client.send(f"FRIEND_REQUESTS|{self.user_id}")

    def handle_message(self, msg):
        # Nếu frame đã destroy thì bỏ qua, tránh lỗi callback sau khi quay lại
        if not hasattr(self, 'friends_listbox') or not self.friends_listbox.winfo_exists():
            return
        if msg.startswith("FRIEND_LIST|"):
            self.render_friend_list(msg)
        elif msg.startswith("FRIEND_REQUESTS|"):
            self.render_friend_requests(msg)
        elif msg.startswith("FRIEND_REQUESTED"):
            self.status_label.config(text="♟️ Đã gửi lời mời kết bạn!")
            self.refresh()
        elif msg.startswith("FRIEND_ACCEPTED"):
            self.status_label.config(text="♞ Đã đồng ý kết bạn!")
            self.refresh()
        elif msg.startswith("FRIEND_DECLINED"):
            self.status_label.config(text="♜ Đã từ chối lời mời!")
            self.refresh()
        elif msg.startswith("ERROR"):
            # Xử lý lỗi protocol trả về từ server
            msg_lower = msg.lower()
            if "friend request already exists" in msg_lower:
                self.status_label.config(text="⚠️ Đã gửi lời mời kết bạn trước đó!")
            elif "user not found" in msg_lower:
                self.status_label.config(text="⚠️ Không tìm thấy người dùng này!")
            elif "cannot add yourself" in msg_lower:
                self.status_label.config(text="⚠️ Không thể kết bạn với chính mình!")
            else:
                # Ẩn debug, chỉ hiện lỗi ngắn gọn
                self.status_label.config(text="Lỗi: " + msg.split("|")[-1].split("[")[0])

    # ================= RENDER =================

    def render_friend_list(self, msg):
        self.friends_listbox.delete(0, tk.END)
        payload = msg.split("|", 1)[1]

        ids = [i.strip() for i in payload.split(",") if i.strip() and i.strip() != str(self.user_id)]

        if not ids:
            self.friends_listbox.insert(tk.END, "♟️ Chưa có bạn bè")
            return

        for fid in ids:
            self.friends_listbox.insert(tk.END, f"♞ Player ID {fid}")

    def render_friend_requests(self, msg):
        for w in self.requests_container.winfo_children():
            w.destroy()

        payload = msg.split("|", 1)[1]
        ids = [i.strip() for i in payload.split(",") if i.strip()]

        if not ids:
            tk.Label(
                self.requests_container,
                text="♔ Không có lời mời nào",
                bg="#ffffff",
                fg="#6b7280"
            ).pack(anchor="w", pady=6)
            return

        for fid in ids:
            row = tk.Frame(self.requests_container, bg="#ffffff", pady=6)
            row.pack(fill="x")

            tk.Label(
                row,
                text=f"♟️ Player ID {fid}",
                font=("Helvetica", 12, "bold"),
                bg="#ffffff",
                fg="#1f2937"
            ).pack(side="left")

            ttk.Button(
                row,
                text="✔ Đồng ý",
                command=lambda f=fid: self.accept_friend(f)
            ).pack(side="right", padx=4)

            ttk.Button(
                row,
                text="✖ Từ chối",
                command=lambda f=fid: self.decline_friend(f)
            ).pack(side="right")

    # ================= ACTIONS =================

    def send_friend_request(self):
        fid = self.search_entry.get().strip()
        if not fid.isdigit() or int(fid) == self.user_id:
            self.status_label.config(text="♜ ID không hợp lệ")
            return
        self.client.send(f"FRIEND_REQUEST|{self.user_id}|{fid}")

    def accept_friend(self, fid):
        self.client.send(f"FRIEND_ACCEPT|{self.user_id}|{fid}")

    def decline_friend(self, fid):
        self.client.send(f"FRIEND_DECLINE|{self.user_id}|{fid}")

    def back(self):
        self.frame.destroy()
        # Remove this UI from listeners to avoid callbacks after destroy
        if hasattr(self.master, 'remove_listener'):
            self.master.remove_listener(self)
        # Restore fullscreen when returning to main menu
        import sys
        if sys.platform == "darwin":
            self.master.attributes("-fullscreen", True)
        else:
            self.master.state("zoomed")
        self.on_back()
