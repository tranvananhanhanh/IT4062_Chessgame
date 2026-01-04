import tkinter as tk
import sys

PIECE_UNICODE = {
    'r': '\u265C', 'n': '\u265E', 'b': '\u265D', 'q': '\u265B', 'k': '\u265A', 'p': '\u265F',
    'R': '\u2656', 'N': '\u2658', 'B': '\u2657', 'Q': '\u2655', 'K': '\u2654', 'P': '\u2659',
    '.': ''
}
START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR"

# ===== Bảng màu pastel hiện đại =====
BG_MAIN = "#f8f5ff"
BG_BOARD = "#fde2f3"
BG_PANEL = "#e3f2fd"
COLOR_LIGHT = "#fce7f3"
COLOR_DARK = "#a6c8ff"
COLOR_SELECTED = "#ff8fab"
TEXT_MAIN = "#2b2b2b"
BTN_EXIT = "#ff6b81"

class GameBotUI:
    def __init__(self, master, match_id, client, on_back, difficulty=None):
        self.master = master
        self.match_id = match_id
        self.client = client
        self.on_back = on_back
        self.difficulty = difficulty or "easy"

        self.master.configure(bg=BG_MAIN)
        self.frame = tk.Frame(master, bg=BG_MAIN)
        self.frame.pack(fill="both", expand=True)

        # Cho phép self.frame mở rộng
        self.frame.rowconfigure(0, weight=1)
        self.frame.columnconfigure(0, weight=1)

        # ====== MAIN CONTAINER ======
        container = tk.Frame(self.frame, bg=BG_MAIN)
        container.grid(row=0, column=0, sticky="nsew")
        container.rowconfigure(0, weight=1)
        # Cả 2 cột đều không giãn ngang
        container.columnconfigure(0, weight=0)
        container.columnconfigure(1, weight=0)

        # ====== BOARD (LEFT) ======
        BOARD_SIZE = 850
        self.board_frame = tk.Frame(
            container, bg=BG_BOARD, bd=4, relief="ridge", width=BOARD_SIZE, height=BOARD_SIZE
        )
        self.board_frame.grid(row=0, column=0, padx=(20, 10), pady=20, sticky="n")
        self.board_frame.grid_propagate(False)

        # ====== INFO PANEL (RIGHT) ======
        INFO_WIDTH = 500
        self.info_frame = tk.Frame(container, bg=BG_PANEL, width=INFO_WIDTH)
        self.info_frame.grid(row=0, column=1, sticky="ns", padx=(10, 20), pady=20)
        self.info_frame.grid_propagate(False)

        tk.Label(
            self.info_frame,
            text="THÔNG TIN VÁN ĐẤU",
            font=("Helvetica", 18, "bold"),
            bg=BG_PANEL,
            fg=TEXT_MAIN
        ).pack(pady=(15, 10))

        self.move_label = tk.Label(
            self.info_frame,
            text="Nước đi bạn: -",
            font=("Helvetica", 14),
            bg=BG_PANEL
        )
        self.move_label.pack(anchor="w", padx=20, pady=6)

        self.bot_label = tk.Label(
            self.info_frame,
            text="Bot: -",
            font=("Helvetica", 14),
            bg=BG_PANEL
        )
        self.bot_label.pack(anchor="w", padx=20, pady=6)

        self.result_label = tk.Label(
            self.info_frame,
            text="Kết quả: Đang chơi",
            font=("Helvetica", 14, "bold"),
            bg=BG_PANEL
        )
        self.result_label.pack(anchor="w", padx=20, pady=12)

        self.time_label = tk.Label(
            self.info_frame,
            text="⏱ Thời gian: ∞",
            font=("Helvetica", 13),
            bg=BG_PANEL
        )
        self.time_label.pack(anchor="w", padx=20, pady=6)

        tk.Button(
            self.info_frame,
            text="THOÁT TRẬN",
            font=("Helvetica", 14, "bold"),
            bg=BTN_EXIT,
            fg="white",
            relief="flat",
            command=self.back
        ).pack(side="bottom", pady=20, ipadx=10, ipady=6)

        # ===== BOARD STATE =====
        self.board_state = self.fen_to_board(START_FEN)
        self.squares = []
        self.selected = None
        self.last_move = "-"
        self.last_bot_move = "-"
        self.draw_board()
        self.start_polling()
        self.game_end_shown = False  # Ngăn overlay kết thúc hiện nhiều lần

    def fen_to_board(self, fen):
        board = []
        for row in fen.split('/'):
            b_row = []
            for c in row:
                if c.isdigit():
                    b_row.extend(['.'] * int(c))
                else:
                    b_row.append(c)
            board.append(b_row)
        return board

    def draw_board(self):
        for w in self.board_frame.winfo_children():
            w.destroy()
        SQUARE_SIZE = 100  # 900/8 ≈ 112, fixed size for each square
        # Do NOT allow rows/columns to auto-scale
        for i in range(8):
            self.board_frame.rowconfigure(i, weight=0, minsize=SQUARE_SIZE)
            self.board_frame.columnconfigure(i, weight=0, minsize=SQUARE_SIZE)
        for i in range(8):
            for j in range(8):
                base_color = COLOR_LIGHT if (i + j) % 2 == 0 else COLOR_DARK
                if self.selected == (i, j):
                    base_color = COLOR_SELECTED
                piece = self.board_state[i][j]
                lbl = tk.Label(
                    self.board_frame,
                    bg=base_color,
                    font=("Arial Unicode MS", 28),
                    text=PIECE_UNICODE.get(piece, ''),
                    relief="flat",
                    borderwidth=0,
                    width=3,  # fixed width
                    height=2  # fixed height
                )
                lbl.grid(row=i, column=j, sticky="nsew")
                lbl.bind("<Button-1>", lambda e, r=i, c=j: self.on_square_click(r, c))
        # Do NOT resize board_frame here!

    def on_square_click(self, row, col):
        # Chỉ cho phép đi khi đến lượt trắng
        if not self.is_white_turn():
            self.result_label.config(text="Chờ bot đi...", fg="#ff6b81")
            return
        piece = self.board_state[row][col]
        if self.selected is None:
            if piece in 'RNBQKP':
                self.selected = (row, col)
                self.draw_board()
            else:
                self.result_label.config(text="Chỉ được chọn quân trắng!", fg="#ff6b81")
        else:
            # Nếu bấm lại đúng ô đang chọn thì bỏ chọn (deselect)
            if self.selected == (row, col):
                self.selected = None
                self.draw_board()
                return
            from_row, from_col = self.selected
            to_row, to_col = row, col
            moving_piece = self.board_state[from_row][from_col]
            # Nếu chọn quân trắng khác thì chuyển sang chọn quân mới
            if piece in 'RNBQKP':
                self.selected = (row, col)
                self.draw_board()
                return
            if moving_piece == 'P' and to_row == 0:
                move = self.coords_to_uci(from_row, from_col, to_row, to_col)
                self.show_promotion_popup(move)
                self.selected = None
                self.draw_board()
            else:
                move = self.coords_to_uci(from_row, from_col, to_row, to_col)
                self.selected = None
                self.send_move(move)
                # KHÔNG gọi self.draw_board() ở đây!

    def is_white_turn(self):
        # Lấy FEN hiện tại từ BOT_MOVE_RESULT hoặc self.board_state
        # Giả sử self.last_fen luôn được cập nhật từ BOT_MOVE_RESULT
        if hasattr(self, 'last_fen'):
            return self.last_fen.split(' ')[1] == 'w'
        return True  # Mặc định cho phép đi đầu tiên

    def show_promotion_popup(self, move):
        popup = tk.Toplevel(self.master)
        popup.transient(self.master)
        popup.grab_set()
        popup.geometry("300x120")
        popup.title("Phong tốt")
        popup.configure(bg="#ffe0f7")
        tk.Label(popup, text="Chọn quân muốn phong:", font=("Helvetica", 14), fg="#ff6b81", bg="#ffe0f7").pack(pady=10)
        btn_frame = tk.Frame(popup, bg="#ffe0f7")
        btn_frame.pack(pady=5)
        for piece, label in zip(['q', 'r', 'b', 'n'], ['Hậu', 'Xe', 'Tượng', 'Mã']):
            tk.Button(
                btn_frame, text=label, width=7, font=("Helvetica", 13),
                bg="#ffb3c6", fg="#2b2b2b",
                command=lambda p=piece: self._promote_and_send(move, p, popup)
            ).pack(side="left", padx=5)

    def _promote_and_send(self, move, promotion, popup):
        popup.destroy()
        self.send_move(move + promotion)

    def coords_to_uci(self, fr, fc, tr, tc):
        files = 'abcdefgh'
        ranks = '87654321'
        return f"{files[fc]}{ranks[fr]}{files[tc]}{ranks[tr]}"

    def send_move(self, move):
        self.last_move = move
        self.move_label.config(text=f"Nước đi bạn: {move}")
        # Always send difficulty with BOT_MOVE
        self.client.send(f"BOT_MOVE|{self.match_id}|{move}|{self.difficulty}\n")

    def start_polling(self):
        responses = self.client.poll()
        for resp in responses:
            self.handle_server_message(resp)
        self.master.after(30, self.start_polling)

    def handle_server_message(self, resp):
        resp_lower = resp.lower()
        # Xử lý lỗi protocol trả về từ server
        if resp.startswith("ERROR|"):
            msg = resp[6:] if resp.startswith("ERROR|") else resp
            # Check if result_label still exists before updating
            if hasattr(self, 'result_label') and self.result_label.winfo_exists():
                if "illegal move" in msg.lower() or "invalid move" in msg.lower():
                    self.result_label.config(text="Nước đi không hợp lệ!", fg="#ff6b81")
                    self.show_error("Nước đi không hợp lệ! Hãy thử lại.")
                elif "check" in msg.lower() or "king in check" in msg.lower():
                    self.result_label.config(text="Bạn đang bị chiếu tướng!", fg="#ff6b81")
                    self.show_error("Bạn đang bị chiếu tướng, hãy bảo vệ vua!")
                else:
                    self.result_label.config(text="Lỗi: " + msg.split("[")[0], fg="#ff6b81")
                    self.show_error("Lỗi: " + msg.split("[")[0])
            return
        if resp.startswith("BOT_MOVE_RESULT|"):
            parts = resp.strip().split('|')
            if len(parts) >= 5:
                self.board_state = self.fen_to_board(parts[1])
                self.last_fen = parts[1]
                self.draw_board()
                self.last_bot_move = parts[2]
                self.bot_label.config(text=f"Bot: {parts[2]}")
                self.board_state = self.fen_to_board(parts[3])
                self.last_fen = parts[3]
                self.result_label.config(text="Kết quả: Đang chơi", fg=TEXT_MAIN)
                self.selected = None
                self.draw_board()
                status = parts[4].strip().lower()
                if status in ["white_win", "black_win", "draw", "timeout", "checkmate", "stalemate", "insufficient_material"]:
                    # Chuyển các trạng thái hòa về 'draw' cho UI
                    if status in ["draw", "stalemate", "insufficient_material"]:
                        self.show_game_end("draw")
                    else:
                        self.show_game_end(status)
            else:
                self.bot_label.config(text=f"Bot: {parts[2]}")
                self.board_state = self.fen_to_board(parts[3])
                self.selected = None
                self.draw_board()
        elif resp.startswith("GAME_END|"):
            parts = resp.strip().split('|')
            if len(parts) >= 3 and parts[2].startswith("winner:"):
                winner = parts[2].split(":")[1]
                status = parts[1].strip().lower()
                if winner == "0":
                    self.show_game_end("white_win")
                else:
                    self.show_game_end("black_win")
            elif len(parts) >= 3 and parts[2] == "draw":
                self.show_game_end("draw")
            else:
                self.show_game_end(resp)
        elif (
            resp.startswith("DRAW") or resp.startswith("CHECKMATE") or resp.startswith("STALEMATE")
            or "timeout" in resp_lower or "win" in resp_lower or "lose" in resp_lower or "finished" in resp_lower or "end" in resp_lower
        ):
            # Nếu là các thông báo hòa, thắng, thua dạng protocol, chuyển thành status chuẩn
            if resp_lower.startswith("draw") or "insufficient_material" in resp_lower:
                self.show_game_end("draw")
            elif resp_lower.startswith("checkmate"):
                self.show_game_end("checkmate")
            elif resp_lower.startswith("stalemate"):
                self.show_game_end("stalemate")
            elif "white_win" in resp_lower:
                self.show_game_end("white_win")
            elif "black_win" in resp_lower:
                self.show_game_end("black_win")
            else:
                self.show_game_end(resp)

    def show_game_end(self, status):
        if getattr(self, 'game_end_shown', False):
            return  # Đã hiện overlay rồi, không hiện lại nữa
        self.game_end_shown = True
        # Đảm bảo luôn cập nhật bàn cờ cuối cùng trước khi hiển thị overlay
        self.board_state = self.fen_to_board(self.last_fen)
        self.draw_board()
        # Chỉ tạo overlay popup, KHÔNG tạo overlay_bg nền đen toàn màn hình nữa
        overlay = tk.Toplevel(self.master)
        overlay.transient(self.master)
        overlay.grab_set()
        overlay.geometry("480x320")
        overlay.title("")
        overlay.configure(bg="#ffe0f7")
        overlay.resizable(False, False)
        overlay.lift()
        result = ""
        reason = ""
        icon = ""
        status = status.lower() if isinstance(status, str) else str(status).lower()
        if status in ["draw", "stalemate", "insufficient_material"]:
            result = "HÒA"
            icon = "\u2694"
            reason = "Ván đấu kết thúc với kết quả hòa."
        elif status in ["white_win", "checkmate"]:
            result = "BẠN THẮNG!"
            icon = "\u265B"
            reason = "Xin chúc mừng, bạn đã chiến thắng!"
        elif status in ["black_win"]:
            result = "BẠN THUA!"
            icon = "\u265A"
            reason = "Rất tiếc, bạn đã thua trận này."
        elif status in ["timeout"]:
            result = "HẾT GIỜ"
            icon = "\u23F1"
            reason = "Trận đấu kết thúc do hết thời gian."
        else:
            result = status
            icon = "\u265F"
            reason = "Trận đấu đã kết thúc."
        frame = tk.Frame(overlay, bg="#ffe0f7", bd=0, highlightthickness=0)
        frame.place(relx=0.5, rely=0.5, anchor="center")
        tk.Label(frame, text=icon, font=("Arial Unicode MS", 80), bg="#ffe0f7", fg="#ff8fab").pack(pady=(18, 0))
        tk.Label(frame, text=result, font=("Helvetica", 28, "bold"), bg="#ffe0f7", fg="#ff6b81").pack(pady=(8, 0))
        tk.Label(frame, text=reason, font=("Helvetica", 15), bg="#ffe0f7", fg="#2b2b2b", wraplength=400, justify="center").pack(pady=(8, 18))
        # Nút quay lại menu chính
        tk.Button(
            frame, text="Quay về menu chính", font=("Helvetica", 15, "bold"), width=20,
            bg="#ff8fab", fg="#232946", activebackground="#f6c177", activeforeground="#232946",
            relief="flat", bd=0, highlightthickness=0,
            command=lambda: self._back_to_home_multi(overlay, None)
        ).pack(pady=(0, 10))
        self.result_label.config(text=f"Kết quả: {result}", fg="#ff6b81")
        # KHÔNG tự động tắt overlay, chỉ tắt khi bấm nút quay lại

    def _back_to_home_multi(self, overlay, overlay_bg):
        overlay.destroy()
        if overlay_bg:
            overlay_bg.destroy()
        self.frame.destroy()
        self.game_end_shown = False  # Reset cờ khi quay về menu
        self.on_back()

    def show_error(self, msg):
        if not hasattr(self, 'error_label'):
            self.error_label = tk.Label(self.info_frame, text="", fg="#ff6b81", bg=BG_PANEL, font=("Helvetica", 13))
            self.error_label.pack(pady=(0, 10))
        self.error_label.config(text=msg)
        self.master.after(2000, lambda: self.error_label.config(text=""))

    def back(self):
        self.frame.destroy()
        self.on_back()
