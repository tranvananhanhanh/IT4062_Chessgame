import tkinter as tk
from tkinter import messagebox
import queue

PIECE_UNICODE = {
    'r': '\u265C', 'n': '\u265E', 'b': '\u265D', 'q': '\u265B', 'k': '\u265A', 'p': '\u265F',
    'R': '\u2656', 'N': '\u2658', 'B': '\u2657', 'Q': '\u2655', 'K': '\u2654', 'P': '\u2659',
    '.': ''
}

START_BOARD_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR"
START_FULL_FEN = START_BOARD_FEN + " w KQkq - 0 1"

BG_MAIN = "#f8f5ff"
BG_BOARD = "#fde2f3"
BG_PANEL = "#e3f2fd"
COLOR_LIGHT = "#fce7f3"
COLOR_DARK = "#a6c8ff"
COLOR_SELECTED = "#ff8fab"
COLOR_HINT = "#e0d9bd"
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
        self.last_fen = START_FULL_FEN
        self.selected = None

        self.game_ended = False
        self.game_end_shown = False

        self.labels = [[None for _ in range(8)] for _ in range(8)]
        self.message_queue = queue.Queue()

        self.build_ui()
        self.draw_board()
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

        tk.Label(self.info_frame, text="CHƠI VỚI BOT",
                 font=("Helvetica", 18, "bold"), bg=BG_PANEL).pack(pady=15)

        self.move_label = tk.Label(self.info_frame, text="Bạn: -", font=("Helvetica", 14), bg=BG_PANEL)
        self.move_label.pack(anchor="w", padx=20)

        self.bot_label = tk.Label(self.info_frame, text="Bot: -", font=("Helvetica", 14), bg=BG_PANEL)
        self.bot_label.pack(anchor="w", padx=20)

        self.result_label = tk.Label(self.info_frame, text="Đang chơi",
                                     font=("Helvetica", 14, "bold"), bg=BG_PANEL)
        self.result_label.pack(anchor="w", padx=20, pady=10)

        self.surrender_btn = tk.Button(
            self.info_frame, text="ĐẦU HÀNG",
            font=("Helvetica", 14, "bold"),
            bg="#adb5bd", command=self.surrender
        )
        self.surrender_btn.pack(pady=10)

        tk.Button(
            self.info_frame, text="THOÁT",
            font=("Helvetica", 14, "bold"),
            bg=BTN_EXIT, fg="black",
            command=self.back
        ).pack(side="bottom", pady=20)

    # ================= BOARD =================
    def fen_to_board(self, board_fen):
        board = []
        for row in board_fen.split('/'):
            r = []
            for c in row:
                if c.isdigit():
                    r.extend(['.'] * int(c))
                else:
                    r.append(c)
            board.append(r[:8])
        return board

    def draw_board(self):
        for r in range(8):
            for c in range(8):
                bg = COLOR_LIGHT if (r + c) % 2 == 0 else COLOR_DARK
                if self.selected == (r, c):
                    bg = COLOR_SELECTED
                elif self.selected and not self.game_ended and self.is_valid_hint_move(*self.selected, r, c):
                    bg = COLOR_HINT

                if not self.labels[r][c]:
                    lbl = tk.Label(
                        self.board_frame,
                        text=PIECE_UNICODE[self.board_state[r][c]],
                        bg=bg, font=("Helvetica", 32),
                        width=3, height=2
                    )
                    lbl.grid(row=r, column=c)
                    lbl.bind("<Button-1>",
                             lambda e, rr=r, cc=c: self.on_square_click(rr, cc))
                    self.labels[r][c] = lbl
                else:
                    self.labels[r][c].config(
                        text=PIECE_UNICODE[self.board_state[r][c]],
                        bg=bg
                    )

    # ================= PROMOTION POPUP =================
    def get_promotion_choice(self):
        """Hiển thị popup để người dùng chọn quân khi phong cấp"""
        promotion_window = tk.Toplevel(self.master)
        promotion_window.title("Phong cấp")
        promotion_window.geometry("320x150")
        promotion_window.resizable(False, False)
        promotion_window.configure(bg=BG_PANEL)
        promotion_window.grab_set()  # Chặn tương tác với cửa sổ chính

        # Đặt popup ở giữa màn hình
        x = self.master.winfo_x() + (self.master.winfo_width() // 2) - 160
        y = self.master.winfo_y() + (self.master.winfo_height() // 2) - 75
        promotion_window.geometry(f"+{x}+{y}")

        choice = tk.StringVar(value="q")

        tk.Label(promotion_window, text="Chọn quân muốn phong cấp:", 
                 font=("Helvetica", 12, "bold"), bg=BG_PANEL).pack(pady=10)

        btn_frame = tk.Frame(promotion_window, bg=BG_PANEL)
        btn_frame.pack(pady=5)

        # q=Queen, r=Rook, b=Bishop, n=Knight
        options = [('q', 'Q'), ('r', 'R'), ('b', 'B'), ('n', 'N')]
        
        for char, display in options:
            btn = tk.Button(btn_frame, text=PIECE_UNICODE[display], font=("Helvetica", 24),
                            width=2, bg="white",
                            command=lambda c=char: [choice.set(c), promotion_window.destroy()])
            btn.pack(side="left", padx=10)

        self.master.wait_window(promotion_window)
        return choice.get()

    # ================= MOVE LOGIC =================
    def is_valid_hint_move(self, fr, fc, tr, tc):
        piece = self.board_state[fr][fc]
        target = self.board_state[tr][tc]
        if piece == '.' or not piece.isupper():
            return False
        if target != '.' and target.isupper():
            return False

        dr, dc = tr - fr, tc - fc
        p = piece.upper()

        if p == 'P':
            if dc == 0 and dr == -1 and target == '.':
                return True
            if fr == 6 and dc == 0 and dr == -2 and target == '.' and self.board_state[5][fc] == '.':
                return True
            if abs(dc) == 1 and dr == -1 and target.islower():
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
        dr = (tr - fr) and (1 if tr > fr else -1)
        dc = (tc - fc) and (1 if tc > fc else -1)
        r, c = fr + dr, fc + dc
        while (r, c) != (tr, tc):
            if self.board_state[r][c] != '.':
                return False
            r += dr
            c += dc
        return True

    # ================= CHECK LOGIC =================
    def find_king(self, board):
        for r in range(8):
            for c in range(8):
                if board[r][c] == 'K':
                    return r, c
        return None

    def square_attacked_by_black(self, board, tr, tc):
        for r in range(8):
            for c in range(8):
                p = board[r][c]
                if p.islower():
                    if self.is_valid_attack(board, r, c, tr, tc):
                        return True
        return False

    def is_valid_attack(self, board, fr, fc, tr, tc):
        piece = board[fr][fc].upper()
        dr, dc = tr - fr, tc - fc
        if piece == 'P':
            return dr == 1 and abs(dc) == 1
        if piece == 'N':
            return (abs(dr), abs(dc)) in [(1, 2), (2, 1)]
        if piece == 'B':
            return abs(dr) == abs(dc) and self.is_path_clear_on_board(board, fr, fc, tr, tc)
        if piece == 'R':
            return (dr == 0 or dc == 0) and self.is_path_clear_on_board(board, fr, fc, tr, tc)
        if piece == 'Q':
            return ((dr == 0 or dc == 0) or abs(dr) == abs(dc)) and \
                   self.is_path_clear_on_board(board, fr, fc, tr, tc)
        if piece == 'K':
            return abs(dr) <= 1 and abs(dc) <= 1
        return False

    def is_path_clear_on_board(self, board, fr, fc, tr, tc):
        dr = (tr - fr) and (1 if tr > fr else -1)
        dc = (tc - fc) and (1 if tc > fc else -1)
        r, c = fr + dr, fc + dc
        while (r, c) != (tr, tc):
            if board[r][c] != '.':
                return False
            r += dr
            c += dc
        return True

    def is_move_legal_considering_check(self, fr, fc, tr, tc):
        tmp = [row[:] for row in self.board_state]
        tmp[tr][tc] = tmp[fr][fc]
        tmp[fr][fc] = '.'
        king_pos = self.find_king(tmp)
        if not king_pos:
            return False
        return not self.square_attacked_by_black(tmp, *king_pos)

    # ================= CLICK =================
    def on_square_click(self, r, c):
        if self.game_ended or not self.is_white_turn():
            return

        if self.selected is None:
            if self.board_state[r][c].isupper():
                self.selected = (r, c)
                self.draw_board()
            return

        fr, fc = self.selected

        if (fr, fc) == (r, c):
            self.selected = None
            self.draw_board()
            return

        if self.board_state[r][c].isupper():
            self.selected = (r, c)
            self.draw_board()
            return

        if self.is_valid_hint_move(fr, fc, r, c):
            if not self.is_move_legal_considering_check(fr, fc, r, c):
                messagebox.showwarning(
                    "Nước đi không hợp lệ",
                    "Không thể đi nước này vì vua sẽ bị chiếu!"
                )
                return

            move = self.coords_to_uci(fr, fc, r, c)
            piece = self.board_state[fr][fc]
            
            # Kiểm tra phong cấp: Tốt trắng (P) đi tới hàng 0
            if piece == 'P' and r == 0:
                promo_char = self.get_promotion_choice()
                move += promo_char
                self.apply_local_move(fr, fc, r, c, promo_char.upper())
            else:
                self.apply_local_move(fr, fc, r, c)

            self.selected = None
            self.draw_board()
            self.send_move(move)

    # ================= SERVER =================
    def send_move(self, move):
        self.move_label.config(text=f"Bạn: {move}")
        self.client.send(f"BOT_MOVE|{self.match_id}|{move}|{self.difficulty}\n")

    def handle_server_message(self, msg):
        if self.game_ended:
            return

        if msg.startswith("GAME_END|"):
            parts = msg.strip().split('|')
            reason = parts[1]
            result = parts[2]
            self.end_game(result, reason)
            return

        if msg.startswith("BOT_MOVE_RESULT|"):
            parts = msg.strip().split('|')
            fen = parts[1]
            bot_move = parts[2]
            status = parts[3]
            
            self.board_state = self.fen_to_board(fen.split()[0])
            self.last_fen = fen
            self.bot_label.config(text=f"Bot: {bot_move}")
            self.selected = None
            self.draw_board()

            if status.lower() != "in_game":
                self.end_game(status)

    def end_game(self, result, reason=None):
        if self.game_ended:
            return

        self.game_ended = True

        if result.lower() == "draw":
            text = "HÒA"
        elif result.lower() == "black_win":
            text = "BẠN THUA"
        elif result.lower() == "white_win":
            text = "BẠN THẮNG"
        else:
            text = result.upper()

        if reason:
            text += f" ({reason})"

        self.result_label.config(text=text)
        self.surrender_btn.config(state="disabled")

        if not self.game_end_shown:
            self.game_end_shown = True
            messagebox.showinfo("Kết thúc ván đấu", text)

    def poll_messages(self):
        try:
            while True:
                self.handle_server_message(self.message_queue.get_nowait())
        except queue.Empty:
            pass
        self.master.after(100, self.poll_messages)

    def is_white_turn(self):
        return self.last_fen.split(' ')[1] == 'w'

    def coords_to_uci(self, fr, fc, tr, tc):
        return f"{'abcdefgh'[fc]}{'87654321'[fr]}{'abcdefgh'[tc]}{'87654321'[tr]}"

    def apply_local_move(self, fr, fc, tr, tc, promo_piece=None):
        if promo_piece:
            self.board_state[tr][tc] = promo_piece
        else:
            self.board_state[tr][tc] = self.board_state[fr][fc]
        self.board_state[fr][fc] = '.'

    def surrender(self):
        if self.game_ended:
            return
        self.client.send(f"SURRENDER|{self.match_id}\n")
        self.surrender_btn.config(state="disabled")

    def back(self):
        self.frame.destroy()
        self.on_back()
