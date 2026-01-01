
import tkinter as tk
from tkinter import messagebox
import queue  # Cho thread-safe message forwarding

PIECE_UNICODE = {
    'r': '\u265C', 'n': '\u265E', 'b': '\u265D', 'q': '\u265B', 'k': '\u265A', 'p': '\u265F',
    'R': '\u2656', 'N': '\u2658', 'B': '\u2657', 'Q': '\u2655', 'K': '\u2654', 'P': '\u2659',
    '.': ''
}

START_BOARD_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR"  # Chỉ board
START_FULL_FEN = START_BOARD_FEN + " w KQkq - 0 1"  # Full cho turn

# ===== COLORS =====
BG_MAIN = "#f8f5ff"
BG_BOARD = "#fde2f3"
BG_PANEL = "#e3f2fd"
COLOR_LIGHT = "#fce7f3"
COLOR_DARK = "#a6c8ff"
COLOR_SELECTED = "#ff8fab"
COLOR_HINT = "#caffbf"
BTN_EXIT = "#ff6b81"


class GameBotUI:
    def __init__(self, master, match_id, client, on_back, difficulty="easy"):
        self.master = master
        self.match_id = match_id
        self.client = client
        self.on_back = on_back
        self.difficulty = difficulty

        self.frame = tk.Frame(master, bg=BG_MAIN)
        self.frame.pack(fill="both", expand=True)

        self.board_state = self.fen_to_board(START_BOARD_FEN)
        self.last_fen = START_FULL_FEN  # Full cho turn
        self.selected = None
        self.game_end_shown = False
        self.game_ended = False  # ← THÊM: Flag để khóa board và buttons

        # Lưu Label từng ô để update mượt
        self.labels = [[None for _ in range(8)] for _ in range(8)]

        # Queue để forward message từ PollClient (thread-safe)
        self.message_queue = queue.Queue()

        self.build_ui()
        self.draw_board()

        # Bắt đầu poll queue ngay
        self.poll_messages()

    # ================= UI =================
    def build_ui(self):
        container = tk.Frame(self.frame, bg=BG_MAIN)
        container.pack(fill="both", expand=True)

        self.board_frame = tk.Frame(container, bg=BG_BOARD, bd=4, relief="ridge")
        self.board_frame.pack(side="left", padx=20, pady=20)

        self.info_frame = tk.Frame(container, bg=BG_PANEL, width=400)
        self.info_frame.pack(side="right", fill="y", padx=20, pady=20)
        self.info_frame.pack_propagate(False)

        tk.Label(
            self.info_frame,
            text="CHƠI VỚI BOT",
            font=("Helvetica", 18, "bold"),
            bg=BG_PANEL
        ).pack(pady=15)

        self.move_label = tk.Label(self.info_frame, text="Bạn: -", font=("Helvetica", 14), bg=BG_PANEL)
        self.move_label.pack(anchor="w", padx=20)

        self.bot_label = tk.Label(self.info_frame, text="Bot: -", font=("Helvetica", 14), bg=BG_PANEL)
        self.bot_label.pack(anchor="w", padx=20)

        self.result_label = tk.Label(self.info_frame, text="Đang chơi", font=("Helvetica", 14, "bold"), bg=BG_PANEL)
        self.result_label.pack(anchor="w", padx=20, pady=10)

        # ← SỬA: Lưu ref button để disable sau
        self.surrender_btn = tk.Button(
            self.info_frame,
            text="ĐẦU HÀNG",
            font=("Helvetica", 14, "bold"),
            bg="#adb5bd",
            command=self.surrender
        )
        self.surrender_btn.pack(pady=10)

        tk.Button(
            self.info_frame,
            text="THOÁT",
            font=("Helvetica", 14, "bold"),
            bg=BTN_EXIT,
            fg="black",  # ← FIX: Đổi từ "white" thành "black" cho chữ đen
            command=self.back
        ).pack(side="bottom", pady=20)

    # ================= BOARD =================
    def fen_to_board(self, board_fen):  # Chỉ nhận board_fen (không full)
        board = []
        for row in board_fen.split('/'):
            r = []
            for c in row:
                if c.isdigit():
                    r.extend(['.'] * int(c))
                else:
                    r.append(c)
            if len(r) != 8:  # Debug: Warn nếu row sai
                print(f"WARN: Row length {len(r)} != 8 in {row}")
            board.append(r[:8])  # Cắt nếu extra (backup)
        return board

    def draw_board(self):
        # Nếu chưa tạo Label thì tạo một lần duy nhất
        if not any(self.labels[0]):
            for r in range(8):
                for c in range(8):
                    bg = COLOR_LIGHT if (r + c) % 2 == 0 else COLOR_DARK
                    lbl = tk.Label(
                        self.board_frame,
                        text=PIECE_UNICODE.get(self.board_state[r][c], ""),
                        bg=bg,
                        font=("Helvetica", 32),
                        width=3,
                        height=2
                    )
                    lbl.grid(row=r, column=c)
                    # ← THÊM: Bind với check game_ended trong on_square_click
                    lbl.bind("<Button-1>", lambda e, rr=r, cc=c: self.on_square_click(rr, cc))
                    self.labels[r][c] = lbl
        else:
            # Cập nhật text và màu cho mỗi ô
            for r in range(8):
                for c in range(8):
                    piece = self.board_state[r][c]
                    bg = COLOR_LIGHT if (r + c) % 2 == 0 else COLOR_DARK
                    if self.selected == (r, c):
                        bg = COLOR_SELECTED
                    elif self.selected and self.is_white_turn() and not self.game_ended:  # ← THÊM: Check game_ended
                        fr, fc = self.selected
                        if self.is_valid_hint_move(fr, fc, r, c):
                            bg = COLOR_HINT
                    self.labels[r][c].config(text=PIECE_UNICODE.get(piece, ""), bg=bg)

    # ================= HINT LOGIC =================
    def is_valid_hint_move(self, fr, fc, tr, tc):
        if self.game_ended:  # ← THÊM: Không hint nếu game end
            return False
        if (fr, fc) == (tr, tc):
            return False

        piece = self.board_state[fr][fc]
        target = self.board_state[tr][tc]

        if piece == '.' or not piece.isupper():
            return False
        if target != '.' and target.isupper():
            return False

        p = piece.upper()
        dr = tr - fr
        dc = tc - fc

        if p == 'P':
            # forward
            if dc == 0:
                if dr == -1 and target == '.':
                    return True
                if fr == 6 and dr == -2 and target == '.' and self.board_state[fr-1][fc] == '.':
                    return True
            # capture - ← FIX: Chỉ cho đi chéo nếu target là quân đen (islower()), không cho empty
            if abs(dc) == 1 and dr == -1 and target != '.' and target.islower():
                return True
            # Promotion moves vẫn valid (hint highlight), dialog sau
            if abs(dr) == 1 and abs(dc) <= 1 and (target == '.' or target.islower()):  # Chỉ forward/capture valid
                return True
            return False

        if p == 'N':
            return (abs(dr), abs(dc)) in [(1, 2), (2, 1)]

        if p == 'B':
            return abs(dr) == abs(dc) and self.is_path_clear(fr, fc, tr, tc)

        if p == 'R':
            return (dr == 0 or dc == 0) and self.is_path_clear(fr, fc, tr, tc)

        if p == 'Q':
            return ((dr == 0 or dc == 0) or abs(dr) == abs(dc)) and \
                   self.is_path_clear(fr, fc, tr, tc)

        if p == 'K':
            return abs(dr) <= 1 and abs(dc) <= 1

        return False

    def is_path_clear(self, fr, fc, tr, tc):
        dr = 1 if tr > fr else -1 if tr < fr else 0
        dc = 1 if tc > fc else -1 if tc < fc else 0

        r, c = fr + dr, fc + dc
        while (r, c) != (tr, tc):
            if self.board_state[r][c] != '.':
                return False
            r += dr
            c += dc
        return True

    # ================= CLICK =================
    def on_square_click(self, row, col):
        if self.game_ended:  # ← THÊM: Khóa click nếu game end
            return

        if not self.is_white_turn():
            return

        piece = self.board_state[row][col]

        if self.selected is None:
            if piece.isupper():
                self.selected = (row, col)
                self.draw_board()
            return

        fr, fc = self.selected

        if (fr, fc) == (row, col):
            self.selected = None
            self.draw_board()
            return

        if piece.isupper():
            self.selected = (row, col)
            self.draw_board()
            return

        if self.is_valid_hint_move(fr, fc, row, col):
            # Check nếu pawn promotion (trắng đến row 0)
            is_pawn_promotion = (self.board_state[fr][fc] == 'P' and row == 0)
            if is_pawn_promotion:
                self.show_promotion_dialog(fr, fc, row, col)
            else:
                move = self.coords_to_uci(fr, fc, row, col)
                # Tạm update local board để UI responsive (di chuyển ngay)
                self.apply_local_move(fr, fc, row, col, promote_to=None)
                self.selected = None
                self.draw_board()
                self.send_move(move)

    # Dialog chọn promotion (giữ nguyên)
    def show_promotion_dialog(self, fr, fc, tr, tc):
        dialog = tk.Toplevel(self.master)
        dialog.title("Phong cấp Tốt")
        dialog.geometry("200x150")
        dialog.transient(self.master)  # Center on main window
        dialog.grab_set()  # Modal (block input)

        tk.Label(dialog, text="Chọn quân cờ để phong cấp:", font=("Helvetica", 12)).pack(pady=10)

        promote_options = ['Q', 'R', 'B', 'N']  # Queen, Rook, Bishop, Knight

        def on_promote(to_piece):
            dialog.destroy()
            # Apply move với promote
            self.apply_local_move(fr, fc, tr, tc, promote_to=to_piece)
            # Tạo UCI với suffix promote
            move = self.coords_to_uci(fr, fc, tr, tc) + to_piece.lower()  # e.g., "a7a8q"
            self.selected = None
            self.draw_board()
            self.send_move(move)

        for opt in promote_options:
            piece_name = {'Q': 'Hậu', 'R': 'Xe', 'B': 'Tượng', 'N': 'Mã'}[opt]
            tk.Button(
                dialog,
                text=f"{piece_name} ({opt})",
                font=("Helvetica", 10, "bold"),
                width=10,
                command=lambda p=opt: on_promote(p),
                bg="#e0e0e0"
            ).pack(pady=5)

    def apply_local_move(self, fr, fc, tr, tc, promote_to=None):
        """Tạm apply move local (user move), handle promotion"""
        piece = self.board_state[fr][fc]
        if promote_to:  # Promotion: Thay pawn bằng piece mới (upper cho trắng)
            self.board_state[tr][tc] = promote_to  # 'Q', 'R', etc.
        else:
            self.board_state[tr][tc] = piece
        self.board_state[fr][fc] = '.'

    def is_white_turn(self):
        if self.game_ended:  # ← THÊM: Không check turn nếu end
            return False
        if self.last_fen and ' ' in self.last_fen:
            return self.last_fen.split(' ')[1] == 'w'
        return True

    def coords_to_uci(self, fr, fc, tr, tc):
        files = "abcdefgh"
        ranks = "87654321"
        return f"{files[fc]}{ranks[fr]}{files[tc]}{ranks[tr]}"

    # ================= SERVER =================
    def send_move(self, move):
        self.move_label.config(text=f"Bạn: {move}")
        self.client.send(f"BOT_MOVE|{self.match_id}|{move}|{self.difficulty}\n")

    def handle_server_message(self, msg):
        print(f"[UI DEBUG] Received and handling: {msg}")  # Debug
        if msg.startswith("BOT_MOVE_RESULT|"):
            parts = msg.strip().split('|')
            if len(parts) == 4:
                _, full_fen, bot_move, status = parts
                board_fen = full_fen.split(' ')[0]  # Chỉ lấy board part
                self.board_state = self.fen_to_board(board_fen)
                self.last_fen = full_fen  # Full cho turn
                self.bot_label.config(text=f"Bot: {bot_move}")
                print(f"[UI DEBUG] Bot move: {bot_move}, FEN board: {board_fen}")

                # Debug: Print board shape và ví dụ piece h5 (row3 col7)
                print(f"Updated board shape: {len(self.board_state)}x{len(self.board_state[0]) if self.board_state else 0}")
                if self.board_state:
                    print(f"h5 piece (row3 col7): '{self.board_state[3][7]}'")  # Nên là 'p' sau h7h5

                # reset selection
                self.selected = None

                # Update UI
                self.draw_board()
                self.master.update_idletasks()  # Force refresh ngay lập tức

                if status.lower() != "in_game":
                    self.end_game(status)
            else:
                print(f"[UI DEBUG] Invalid message parts: {parts}")

        # ← THÊM: Xử lý GAME_END message (e.g., 'GAME_END|stalemate|draw')
        elif msg.startswith("GAME_END|"):
            parts = msg.strip().split('|')
            if len(parts) == 3:
                _, reason, result = parts
                print(f"[UI DEBUG] Game ended: {reason} -> {result}")
                self.end_game(result, reason)  # Truyền result và reason

    # ← THÊM: Method end_game để khóa UI
    def end_game(self, result, reason=None):
        self.game_ended = True  # Khóa flag

        # Disable surrender button
        if hasattr(self, 'surrender_btn'):
            self.surrender_btn.config(state="disabled")

        # Unbind clicks trên tất cả labels (khóa board)
        for r in range(8):
            for c in range(8):
                if self.labels[r][c]:
                    self.labels[r][c].unbind("<Button-1>")

        # Update result label với chi tiết
        end_text = self.get_end_text(result, reason)
        self.result_label.config(text=end_text)

        # Show messagebox nếu chưa shown
        if not self.game_end_shown:
            self.game_end_shown = True
            messagebox.showinfo("Kết thúc ván đấu", end_text)

        # Clear selection và redraw (màu normal, không hint)
        self.selected = None
        self.draw_board()
        self.master.update_idletasks()

    def get_end_text(self, result, reason=None):
        """Map result/reason sang text tiếng Việt"""
        mapping = {
            "draw": "HÒA",
            "stalemate": "HÒA – BẾ TẮC",
            "checkmate": "CHIẾU HẾT",
            "white_win": "BẠN THẮNG",
            "black_win": "BOT THẮNG"  # ← FIX: Đổi "BẠN THUA" thành "BOT THẮNG" (user trắng, bot thắng)
        }
        base_text = mapping.get(result.lower(), f"KẾT THÚC ({result})")
        if reason and reason.lower() == "stalemate":
            base_text = "HÒA – BẾ TẮC"
        return base_text

    # Poll queue để forward từ PollClient
    def poll_messages(self):
        """Poll message từ queue (thread-safe), gọi handler."""
        try:
            while True:  # Drain tất cả pending messages
                msg = self.message_queue.get_nowait()
                self.handle_server_message(msg)
        except queue.Empty:
            pass  # Không có message mới
        # Poll lại sau 100ms (không block UI)
        self.master.after(100, self.poll_messages)

    # ================= END =================
    def show_game_end(self, status):
        # Deprecated: Dùng end_game thay thế
        pass

    def surrender(self):
        if self.game_ended:  # ← THÊM: Không cho nếu end
            return
        if messagebox.askyesno("Đầu hàng", "Bạn chắc chắn muốn đầu hàng?"):
            self.end_game("black_win")  # Giả sử user trắng, bot thắng

    def back(self):
        if self.game_ended:
            # Vẫn cho thoát dù end
            pass
        self.frame.destroy()
        self.on_back()
