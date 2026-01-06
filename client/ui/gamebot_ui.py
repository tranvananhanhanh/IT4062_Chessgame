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

BG_MAIN = "#f0f0f0"
BG_BOARD_WOOD = "#dcae8a" 
BG_PANEL = "#ffffff"
COLOR_LIGHT = "#ecd9b9"
COLOR_DARK = "#ae8968"
COLOR_SELECTED = "#7b9652" 
COLOR_HINT_SAFE = "#82ad44"  
COLOR_HINT_DANGER = "#ff4d4d" 
BTN_EXIT_BG = "#ff6b81"

class GameBotUI:
    def __init__(self, master, match_id, client, on_back, difficulty="easy"):
        self.master = master
        self.match_id = match_id
        self.client = client
        self.on_back = on_back
        self.difficulty = difficulty

        self.frame = tk.Frame(master, bg=BG_MAIN)
        self.frame.pack(fill="both", expand=True)

        self.frame.columnconfigure(0, weight=3)
        self.frame.columnconfigure(1, weight=1)
        self.frame.rowconfigure(0, weight=1)

        self.board_state = self.fen_to_board(START_BOARD_FEN)
        self.last_fen = START_FULL_FEN
        self.selected = None
        self.game_ended = False
        self.game_end_shown = False

        self.canvases = [[None for _ in range(8)] for _ in range(8)]
        self.piece_texts = [[None for _ in range(8)] for _ in range(8)]
        self.message_queue = queue.Queue()

        self.build_ui()
        self.draw_board()
        self.poll_messages()

    def build_ui(self):
        self.board_container = tk.Frame(self.frame, bg=BG_BOARD_WOOD, bd=0)
        self.board_container.grid(row=0, column=0, sticky="nsew")
        
        self.board_centerer = tk.Frame(self.board_container, bg=BG_BOARD_WOOD)
        self.board_centerer.place(relx=0.5, rely=0.5, anchor="center")

        self.info_frame = tk.Frame(self.frame, bg=BG_PANEL, bd=1, relief="solid")
        self.info_frame.grid(row=0, column=1, sticky="nsew")
        
        tk.Label(self.info_frame, text="CHƠI VỚI BOT",
                 font=("Helvetica", 18, "bold"), bg=BG_PANEL, fg="#333").pack(pady=(40, 20))

        stats_frame = tk.Frame(self.info_frame, bg=BG_PANEL)
        stats_frame.pack(fill="x", padx=20)

        self.move_label = tk.Label(stats_frame, text="Bạn: -", font=("Helvetica", 12), bg=BG_PANEL)
        self.move_label.pack(anchor="w", pady=2)

        self.bot_label = tk.Label(stats_frame, text="Bot: -", font=("Helvetica", 12), bg=BG_PANEL)
        self.bot_label.pack(anchor="w", pady=2)

        self.result_label = tk.Label(self.info_frame, text="Đang chơi",
                                     font=("Helvetica", 12, "italic"), bg=BG_PANEL, fg="#666")
        self.result_label.pack(pady=20)

        self.surrender_btn = tk.Button(
            self.info_frame, text="ĐẦU HÀNG", font=("Helvetica", 10, "bold"),
            bg="#f8f9fa", relief="ridge", command=self.surrender
        )
        self.surrender_btn.pack(pady=10, ipadx=10)

        self.exit_btn = tk.Button(
            self.info_frame, text="THOÁT", font=("Helvetica", 11, "bold"),
            bg=BTN_EXIT_BG, fg="black", relief="flat", cursor="hand2",
            command=self.back
        )
        self.exit_btn.pack(side="bottom", fill="x", padx=40, pady=30, ipady=8)

    def draw_board(self):
        cell_size = 80 
        for r in range(8):
            for c in range(8):
                bg_color = COLOR_LIGHT if (r + c) % 2 == 0 else COLOR_DARK
                if self.selected == (r, c):
                    bg_color = COLOR_SELECTED

                if not self.canvases[r][c]:
                    cvs = tk.Canvas(
                        self.board_centerer, width=cell_size, height=cell_size,
                        bg=bg_color, highlightthickness=0
                    )
                    cvs.grid(row=r, column=c)
                    cvs.bind("<Button-1>", lambda e, rr=r, cc=c: self.on_square_click(rr, cc))
                    self.canvases[r][c] = cvs
                    
                    txt = cvs.create_text(
                        cell_size/2, cell_size/2,
                        text=PIECE_UNICODE[self.board_state[r][c]],
                        font=("Helvetica", 40)
                    )
                    self.piece_texts[r][c] = txt
                else:
                    cvs = self.canvases[r][c]
                    cvs.config(bg=bg_color)
                    cvs.itemconfig(self.piece_texts[r][c], text=PIECE_UNICODE[self.board_state[r][c]])
                    cvs.delete("hint")

                if self.selected and not self.game_ended:
                    if self.is_valid_hint_move(*self.selected, r, c):
                        is_legal = self.is_move_legal_considering_check(*self.selected, r, c)
                        color = COLOR_HINT_SAFE if is_legal else COLOR_HINT_DANGER
                        margin = cell_size * 0.32
                        cvs.create_oval(margin, margin, cell_size - margin, cell_size - margin, outline=color, width=4, tags="hint")

    def on_square_click(self, r, c):
        if self.game_ended or not self.is_white_turn(): return
        if self.selected is None:
            if self.board_state[r][c].isupper():
                self.selected = (r, c)
                self.draw_board()
            return
        fr, fc = self.selected
        if (fr, fc) == (r, c) or self.board_state[r][c].isupper():
            self.selected = (r, c) if (self.board_state[r][c].isupper() and (fr, fc) != (r, c)) else None
            self.draw_board()
            return
        if self.is_valid_hint_move(fr, fc, r, c):
            if not self.is_move_legal_considering_check(fr, fc, r, c): return
            move = self.coords_to_uci(fr, fc, r, c)
            if self.board_state[fr][fc] == 'P' and r == 0:
                promo = self.get_promotion_choice()
                move += promo
                self.apply_local_move(fr, fc, r, c, promo.upper())
            else: self.apply_local_move(fr, fc, r, c)
            self.selected = None
            self.draw_board()
            self.send_move(move)

    def get_promotion_choice(self):
        # Use a simple, reliable promotion dialog (text labels avoid missing fonts)
        promotion_window = tk.Toplevel(self.master)
        promotion_window.title("Phong cấp")
        promotion_window.configure(bg="#f9f9f9")
        promotion_window.resizable(False, False)
        promotion_window.transient(self.master)

        # Ensure window is mapped before grab to avoid "window not viewable"
        promotion_window.update_idletasks()
        try:
            promotion_window.wait_visibility()
            promotion_window.grab_set()
        except tk.TclError:
            # Fallback: defer grab to the next idle cycle
            promotion_window.after_idle(lambda: promotion_window.grab_set())

        # Center on parent
        self.master.update_idletasks()
        x = self.master.winfo_x() + (self.master.winfo_width() // 2) - 180
        y = self.master.winfo_y() + (self.master.winfo_height() // 2) - 90
        # Tăng chiều cao để luôn thấy đủ 4 lựa chọn (có Mã/N)
        promotion_window.geometry("360x230+%d+%d" % (x, y))

        choice = tk.StringVar(value="q")

        tk.Label(
            promotion_window,
            text="Chọn quân phong cấp",
            font=("Helvetica", 12, "bold"),
            bg="#f9f9f9",
            fg="#333"
        ).pack(pady=(14, 10))

        btn_frame = tk.Frame(promotion_window, bg="#f9f9f9")
        btn_frame.pack(pady=6)

        options = [
            ("Hậu (Q)", "q"),
            ("Xe (R)", "r"),
            ("Tượng (B)", "b"),
            ("Mã (N)", "n"),
        ]

        for label, char in options:
            tk.Button(
                btn_frame,
                text=label,
                font=("Helvetica", 11, "bold"),
                width=14,
                command=lambda c=char: (choice.set(c), promotion_window.destroy())
            ).pack(fill="x", pady=4)

        promotion_window.focus_force()
        self.master.wait_window(promotion_window)
        return choice.get()

    def surrender(self):
        if self.game_ended: return
        self.client.send(f"SURRENDER|{self.match_id}\n")
        self.end_game("black_win", "Đã đầu hàng")

    def send_move(self, move):
        self.move_label.config(text=f"Bạn: {move}")
        self.client.send(f"BOT_MOVE|{self.match_id}|{move}|{self.difficulty}\n")

    def fen_to_board(self, board_fen):
        board = []
        for row in board_fen.split('/'):
            r = []
            for c in row:
                if c.isdigit(): r.extend(['.'] * int(c))
                else: r.append(c)
            board.append(r[:8])
        return board

    def is_valid_hint_move(self, fr, fc, tr, tc):
        piece = self.board_state[fr][fc]
        target = self.board_state[tr][tc]
        if piece == '.' or not piece.isupper(): return False
        if target != '.' and target.isupper(): return False
        dr, dc = tr - fr, tc - fc
        p = piece.upper()
        if p == 'P':
            if dc == 0 and dr == -1 and target == '.': return True
            if fr == 6 and dc == 0 and dr == -2 and target == '.' and self.board_state[5][fc] == '.': return True
            if abs(dc) == 1 and dr == -1 and target.islower(): return True
            return False
        if p == 'N': return (abs(dr), abs(dc)) in [(1, 2), (2, 1)]
        if p == 'B': return abs(dr) == abs(dc) and self.is_path_clear(fr, fc, tr, tc)
        if p == 'R': return (dr == 0 or dc == 0) and self.is_path_clear(fr, fc, tr, tc)
        if p == 'Q': return ((dr == 0 or dc == 0) or abs(dr) == abs(dc)) and self.is_path_clear(fr, fc, tr, tc)
        if p == 'K': return abs(dr) <= 1 and abs(dc) <= 1
        return False

    def is_path_clear(self, fr, fc, tr, tc):
        dr = (tr - fr) and (1 if tr > fr else -1)
        dc = (tc - fc) and (1 if tc > fc else -1)
        r, c = fr + dr, fc + dc
        while (r, c) != (tr, tc):
            if self.board_state[r][c] != '.': return False
            r += dr; c += dc
        return True

    def find_king(self, board):
        for r in range(8):
            for c in range(8):
                if board[r][c] == 'K': return r, c
        return None

    def square_attacked_by_black(self, board, tr, tc):
        for r in range(8):
            for c in range(8):
                if board[r][c].islower() and self.is_valid_attack(board, r, c, tr, tc): return True
        return False

    def is_valid_attack(self, board, fr, fc, tr, tc):
        piece = board[fr][fc].upper()
        dr, dc = tr - fr, tc - fc
        if piece == 'P': return dr == 1 and abs(dc) == 1
        if piece == 'N': return (abs(dr), abs(dc)) in [(1, 2), (2, 1)]
        if piece == 'B': return abs(dr) == abs(dc) and self.is_path_clear_on_board(board, fr, fc, tr, tc)
        if piece == 'R': return (dr == 0 or dc == 0) and self.is_path_clear_on_board(board, fr, fc, tr, tc)
        if piece == 'Q': return ((dr == 0 or dc == 0) or abs(dr) == abs(dc)) and self.is_path_clear_on_board(board, fr, fc, tr, tc)
        if piece == 'K': return abs(dr) <= 1 and abs(dc) <= 1
        return False

    def is_path_clear_on_board(self, board, fr, fc, tr, tc):
        dr = (tr - fr) and (1 if tr > fr else -1)
        dc = (tc - fc) and (1 if tc > fc else -1)
        r, c = fr + dr, fc + dc
        while (r, c) != (tr, tc):
            if board[r][c] != '.': return False
            r += dr; c += dc
        return True

    def is_move_legal_considering_check(self, fr, fc, tr, tc):
        tmp = [row[:] for row in self.board_state]
        tmp[tr][tc] = tmp[fr][fc]; tmp[fr][fc] = '.'
        k_pos = self.find_king(tmp)
        return not self.square_attacked_by_black(tmp, *k_pos) if k_pos else False

    def poll_messages(self):
        try:
            while True:
                msg = self.message_queue.get_nowait()
                if self.game_ended: continue
                if msg.startswith("GAME_END|"):
                    p = msg.strip().split('|')
                    self.end_game(p[2], p[1])
                elif msg.startswith("BOT_MOVE_RESULT|"):
                    p = msg.strip().split('|')
                    self.board_state = self.fen_to_board(p[1].split()[0])
                    self.last_fen = p[1]; self.bot_label.config(text=f"Bot: {p[2]}")
                    self.selected = None; self.draw_board()
                    if p[3].lower() != "in_game": self.end_game(p[3])
        except queue.Empty: pass
        self.master.after(100, self.poll_messages)

    # ĐÃ SỬA LỖI LOGIC HIỂN THỊ TẠI ĐÂY
    def end_game(self, result, reason=None):
        if self.game_ended: return
        self.game_ended = True
        res = result.lower()
        
        # Phân loại rõ rệt kết quả dựa trên status từ server
        if res == "white_win":
            text = "BẠN THẮNG"
        elif res == "black_win":
            text = "BẠN THUA"
        elif res == "draw" or "stalemate" in res or "material" in res:
            text = "HÒA"
        else:
            text = res.upper()
            
        if reason:
            text += f" ({reason})"
            
        self.result_label.config(text=text, fg="red", font=("Helvetica", 12, "bold"))
        self.surrender_btn.config(state="disabled")
        
        if not self.game_end_shown:
            self.game_end_shown = True
            messagebox.showinfo("Kết thúc", text)
            self.back() # Tự động quay về menu chính

    def is_white_turn(self): return self.last_fen.split(' ')[1] == 'w'
    def coords_to_uci(self, fr, fc, tr, tc): return f"{'abcdefgh'[fc]}{'87654321'[fr]}{'abcdefgh'[tc]}{'87654321'[tr]}"
    def apply_local_move(self, fr, fc, tr, tc, promo=None):
        self.board_state[tr][tc] = promo if promo else self.board_state[fr][fc]
        self.board_state[fr][fc] = '.'
    def back(self):
        self.frame.destroy(); self.on_back()
