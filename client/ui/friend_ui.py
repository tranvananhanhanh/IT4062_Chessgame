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
        elif sys.platform == "win32":
            self.master.state("zoomed")
        else:
            self.master.attributes('-zoomed', True)
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

        self.friends_listbox.bind("<Double-Button-1>", self.on_friend_double_click)

        # ================= RIGHT: FRIEND REQUESTS + CHAT PANEL =================
        self.right_panel = tk.Frame(content, bg="#eef6ff")
        self.right_panel.grid(row=0, column=1, sticky="nsew")
        self.right_panel.rowconfigure(0, weight=1)
        self.right_panel.columnconfigure(0, weight=1)

        # Chat display (ẩn mặc định)
        self.chat_display = tk.Text(self.right_panel, state="disabled", height=12, bg="#fff", fg="#222", font=("Helvetica", 12))
        self.chat_display.grid(row=0, column=0, sticky="nsew", padx=10, pady=(10, 0))
        self.chat_display.grid_remove()

        # Entry + send (ẩn mặc định)
        self.entry_frame = tk.Frame(self.right_panel, bg="#eef6ff")
        self.entry = tk.Entry(self.entry_frame, font=("Helvetica", 12))
        self.entry.pack(side="left", fill="x", expand=True, padx=(0, 8))
        self.entry.bind("<Return>", lambda e: self.send_chat_to_current())  # Press Enter to send
        self.send_btn = tk.Button(self.entry_frame, text="Gửi", font=("Helvetica", 11, "bold"), command=self.send_chat_to_current)
        self.send_btn.pack(side="right")
        self.entry_frame.grid(row=1, column=0, sticky="ew", padx=10, pady=10)
        self.entry_frame.grid_remove()

        # Friend requests container giữ nguyên
        self.requests_container = tk.Frame(self.right_panel, bg="#ffffff")
        self.requests_container.grid(row=2, column=0, sticky="ew", padx=10, pady=10)

        self.current_chat_id = None
        self._chat_history = {}  # id -> list of messages
        self.id_to_name = {}  # user_id (str) -> username
        self.name_to_id = {}  # username -> user_id (str)

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
        print("[DEBUG] FriendUI received:", repr(msg))  # Debug print for all incoming messages
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
        elif msg.startswith("CHAT_FROM|"):
            parts = msg.strip().split("|", 2)
            print("[DEBUG] CHAT_FROM parts:", parts)  # Debug print for chat message parts
            if len(parts) == 3:
                from_id, content = parts[1], parts[2]
                # Get friend name for better display
                friend_name = self.id_to_name.get(from_id, f"User {from_id}")
                
                # from_id bây giờ là user_id (server đã sửa)
                if from_id not in self._chat_history:
                    self._chat_history[from_id] = []
                self._chat_history[from_id].append(f"{friend_name}: {content}\n")
                if self.current_chat_id == from_id:
                    self.chat_display.config(state="normal")
                    self.chat_display.insert(tk.END, f"{friend_name}: {content}\n")
                    self.chat_display.config(state="disabled")
                    self.chat_display.see(tk.END)  # Auto-scroll to bottom

    # ================= RENDER =================

    def render_friend_list(self, msg):
        self.friends_listbox.delete(0, tk.END)
        payload = msg.split("|", 1)[1]
        # Server trả về: 'id:name:state,id:name:state,...'
        if not payload.strip():
            self.friends_listbox.insert(tk.END, "♟️ Chưa có bạn bè")
            return
        
        entries = [e.strip() for e in payload.split(",") if e.strip()]
        if not entries:
            self.friends_listbox.insert(tk.END, "♟️ Chưa có bạn bè")
            return
        
        for entry in entries:
            parts = entry.split(":")
            if len(parts) >= 3:
                fid, fname, fstate = parts[0], parts[1], parts[2]
                self.id_to_name[fid] = fname
                self.name_to_id[fname] = fid
                
                # Hiển thị state với màu
                if fstate == "online":
                    color_dot = "🟢"  # Xanh
                elif fstate == "ingame":
                    color_dot = "🔴"  # Đỏ
                else:  # offline
                    color_dot = "⚫"  # Đen
                
                self.friends_listbox.insert(tk.END, f"{color_dot} {fname} (ID: {fid})")
            elif len(parts) >= 2:
                fid, fname = parts[0], parts[1]
                self.id_to_name[fid] = fname
                self.name_to_id[fname] = fid
                self.friends_listbox.insert(tk.END, f"♞ {fname} (ID: {fid})")
            else:
                fid = parts[0]
                self.id_to_name[fid] = fid
                self.name_to_id[fid] = fid
                self.friends_listbox.insert(tk.END, f"♞ Player ID {fid}")

    def render_friend_requests(self, msg):
        for w in self.requests_container.winfo_children():
            w.destroy()

        payload = msg.split("|", 1)[1]
        # Server trả về: 'id:name,id:name,...'
        if not payload.strip():
            tk.Label(
                self.requests_container,
                text="♔ Không có lời mời nào",
                bg="#ffffff",
                fg="#6b7280"
            ).pack(anchor="w", pady=6)
            return
        
        entries = [e.strip() for e in payload.split(",") if e.strip()]
        if not entries:
            tk.Label(
                self.requests_container,
                text="♔ Không có lời mời nào",
                bg="#ffffff",
                fg="#6b7280"
            ).pack(anchor="w", pady=6)
            return

        for entry in entries:
            parts = entry.split(":")
            if len(parts) >= 2:
                fid, fname = parts[0], parts[1]
                display_name = f"{fname} (ID: {fid})"
            else:
                fid = entry
                display_name = f"Player ID {fid}"
            
            row = tk.Frame(self.requests_container, bg="#ffffff", pady=6)
            row.pack(fill="x")

            tk.Label(
                row,
                text=f"♟️ {display_name}",
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
        elif sys.platform == "win32":
            self.master.state("zoomed")
        else:
            self.master.attributes('-zoomed', True)
        self.on_back()

    def on_friend_double_click(self, event):
        selection = self.friends_listbox.curselection()
        if not selection:
            return
        friend_text = self.friends_listbox.get(selection[0])
        # Extract friend id from text (format: '{color_dot} {fname} (ID: {fid})')
        if "(ID:" in friend_text:
            # Extract the ID part: "... (ID: 123)" -> "123"
            id_part = friend_text.split("(ID:")[-1].strip().rstrip(")")
            self.show_chat_panel(id_part)
        else:
            print(f"[Friend] Cannot parse friend ID from: {friend_text}")

    def show_chat_panel(self, friend_id):
        self.current_chat_id = friend_id
        self.chat_display.grid()
        self.entry_frame.grid()
        self.entry.delete(0, tk.END)
        
        # Update chat title with friend name
        friend_name = self.id_to_name.get(friend_id, f"User {friend_id}")
        
        # Hiển thị lịch sử chat nếu có
        self.chat_display.config(state="normal")
        self.chat_display.delete(1.0, tk.END)
        self.chat_display.insert(tk.END, f"=== Chat với {friend_name} ===\n\n")
        if friend_id in self._chat_history:
            for msg in self._chat_history[friend_id]:
                self.chat_display.insert(tk.END, msg)
        self.chat_display.config(state="disabled")
        self.chat_display.see(tk.END)  # Scroll to bottom
        self.entry.focus_set()

    def send_chat_to_current(self):
        msg = self.entry.get().strip()
        if msg and self.current_chat_id:
            # Don't add \n - PollClient will add it automatically
            self.client.send(f"CHAT|{self.current_chat_id}|{msg}")
            print(f"[Friend] Sending chat to {self.current_chat_id}: {msg}")
            self.chat_display.config(state="normal")
            self.chat_display.insert(tk.END, f"Bạn: {msg}\n")
            self.chat_display.config(state="disabled")
            self.chat_display.see(tk.END)  # Auto-scroll to bottom
            if self.current_chat_id not in self._chat_history:
                self._chat_history[self.current_chat_id] = []
            self._chat_history[self.current_chat_id].append(f"Bạn: {msg}\n")
            self.entry.delete(0, tk.END)
