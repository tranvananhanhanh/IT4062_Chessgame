import tkinter as tk
from tkinter import ttk

FONT_TITLE = ("Helvetica", 16, "bold")
FONT_LABEL = ("Helvetica", 11)
FONT_BUTTON = ("Helvetica", 11, "bold")

class LeaderboardUI:
    def __init__(self, master, client, on_back):
        self.frame = tk.Frame(master, padx=20, pady=20)
        self.frame.pack(expand=True, fill="both")
        self.client = client
        self.on_back = on_back
        self.leaderboard_data = []

        # Title
        tk.Label(self.frame, text="🏆 Bảng Xếp Hạng (Top 10)", font=FONT_TITLE, fg="#d4af37").pack(pady=(0, 20))

        # Treeview for leaderboard
        self.tree_frame = tk.Frame(self.frame)
        self.tree_frame.pack(fill="both", expand=True, pady=(0, 20))

        # Columns
        columns = ("Hạng", "Tên", "ELO", "Thắng", "Thua", "Hòa")
        self.tree = ttk.Treeview(self.tree_frame, columns=columns, height=10, show="headings")
        
        # Define column headings and widths
        self.tree.column("#0", width=0, stretch=False)
        self.tree.column("Hạng", anchor="center", width=60)
        self.tree.column("Tên", anchor="w", width=150)
        self.tree.column("ELO", anchor="center", width=80)
        self.tree.column("Thắng", anchor="center", width=80)
        self.tree.column("Thua", anchor="center", width=80)
        self.tree.column("Hòa", anchor="center", width=80)

        for col in columns:
            self.tree.heading(col, text=col)

        # Scrollbar
        scrollbar = ttk.Scrollbar(self.tree_frame, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscroll=scrollbar.set)

        self.tree.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        # Status label
        self.status_label = tk.Label(self.frame, text="Đang tải dữ liệu...", font=FONT_LABEL, fg="#666")
        self.status_label.pack(pady=(10, 10))

        # Back button
        back_btn = tk.Button(self.frame, text="Quay lại", font=FONT_BUTTON, width=20, pady=5, command=self._back)
        back_btn.pack()

        # Request leaderboard data
        self._request_leaderboard()

    def _request_leaderboard(self):
        """Gửi yêu cầu lấy top 10 leaderboard"""
        self.client.send("GET_LEADERBOARD|10\n")

    def update_leaderboard(self, data):
        """Cập nhật dữ liệu leaderboard
        data format: list of tuples (rank, username, elo, wins, losses, draws)
        """
        # Clear old data
        for item in self.tree.get_children():
            self.tree.delete(item)

        # Insert new data
        for rank, username, elo, wins, losses, draws in data:
            # Color code based on rank
            if rank <= 3:
                tags = ("medal",)
            else:
                tags = ()
            
            self.tree.insert("", "end", values=(rank, username, elo, wins, losses, draws), tags=tags)

        # Configure tag colors
        self.tree.tag_configure("medal", foreground="#d4af37", font=("Helvetica", 11, "bold"))

        self.status_label.config(text="Top 10 người chơi xuất sắc nhất", fg="#2b2b2b")

    def show_error(self, message):
        """Hiển thị lỗi"""
        self.status_label.config(text=f"Lỗi: {message}", fg="red")

    def _back(self):
        self.on_back()

    def destroy(self):
        self.frame.destroy()
