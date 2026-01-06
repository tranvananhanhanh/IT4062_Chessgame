import tkinter as tk
from tkinter import messagebox
from ui.notifications import show_result_overlay, show_toast
from ui.gamecontrol_ui import GameControlUI
from ui.game_chat_ui import GameChatUI

class PvPUI:
    def __init__(self, master, match_id, my_color, opponent_name, client, on_back, user_id):
        self.master = master
        self.match_id = match_id
        self.my_color = my_color
        self.opponent_name = opponent_name
        self.client = client
        self.on_back = on_back
        self.user_id = user_id
        
        # Timer state
        self.white_time = 10 * 60  # 10 minutes
        self.black_time = 10 * 60
        self.timer_running = False
        self.timer_id = None
        
        self.frame = tk.Frame(master)
        self.frame.pack(fill="both", expand=True)
        
        # Board state
        self.board_state = self.init_board()
        self.selected = None
        self.last_move = None
        self.is_my_turn = (my_color == "white")  # White starts first
        
        # UI Components
        self.setup_ui()
        self.draw_board()
        
        # Start timer
        self.start_timer()
    
    def destroy(self):
        """Cleanup method to stop timers and destroy frame"""
        self.stop_timer()
        if hasattr(self, 'frame') and self.frame:
            self.frame.destroy()
        
    def init_board(self):
        """Initialize standard chess board
        board[0] = rank 8 (Black: rnbqkbnr)
        board[1] = rank 7 (Black pawns: pppppppp)
        ...
        board[6] = rank 2 (White pawns: PPPPPPPP)
        board[7] = rank 1 (White: RNBQKBNR)
        """
        board = [['' for _ in range(8)] for _ in range(8)]
        
        # Black pieces at top (rank 8, 7)
        pieces = ['r', 'n', 'b', 'q', 'k', 'b', 'n', 'r']
        for i in range(8):
            board[0][i] = pieces[i]  # rank 8: black pieces (lowercase)
            board[1][i] = 'p'        # rank 7: black pawns
        
        # White pieces at bottom (rank 2, 1)
        for i in range(8):
            board[6][i] = 'P'        # rank 2: white pawns
            board[7][i] = pieces[i].upper()  # rank 1: white pieces (UPPERCASE)
        
        return board
        
    def setup_ui(self):
        # Top info bar
        info_frame = tk.Frame(self.frame, bg="#2b2b2b", height=60)
        info_frame.pack(side="top", fill="x")
        info_frame.pack_propagate(False)
        
        tk.Label(info_frame, text=f"Match ID: {self.match_id}", 
                font=("Helvetica", 12, "bold"), bg="#2b2b2b", fg="white").pack(side="left", padx=20)
        tk.Label(info_frame, text=f"You: {self.my_color.upper()}", 
                font=("Helvetica", 12, "bold"), bg="#2b2b2b", fg="white").pack(side="left", padx=20)
        tk.Label(info_frame, text=f"Opponent: {self.opponent_name}", 
                font=("Helvetica", 12, "bold"), bg="#2b2b2b", fg="white").pack(side="left", padx=20)
        
        # Timer labels
        self.white_timer_label = tk.Label(info_frame, text="White: 10:00", 
                                         font=("Helvetica", 12, "bold"), bg="#2b2b2b", fg="white")
        self.white_timer_label.pack(side="right", padx=20)
        
        self.black_timer_label = tk.Label(info_frame, text="Black: 10:00", 
                                         font=("Helvetica", 12, "bold"), bg="#2b2b2b", fg="white")
        self.black_timer_label.pack(side="right", padx=20)
        
        # Main container
        container = tk.Frame(self.frame, bg="#f0f0f0")
        container.pack(fill="both", expand=True)
        
        # Left panel - Board
        left_panel = tk.Frame(container, bg="#f0f0f0")
        left_panel.pack(side="left", fill="both", expand=True, padx=20, pady=20)
        
        self.canvas = tk.Canvas(left_panel, width=600, height=600, bg="#ffffff")
        self.canvas.pack()
        self.canvas.bind("<Button-1>", self.on_click)

        # Center panel - Chat
        self.game_chat = GameChatUI(
            container,
            self.match_id,
            self.client
        )
        self.game_chat.pack(side="left", fill="y", padx=10, pady=20)

        # Right panel - Controls (tách ra GameControlUI)
        self.game_control = GameControlUI(
            container,
            self.my_color,
            self.match_id,
            self.user_id,
            self.client,
            self.back_to_menu
        )
        self.game_control.pack(side="right", fill="y", padx=(0, 20), pady=20)
        
        # Liên kết label để cập nhật trạng thái
        self.status_label = self.game_control.status_label
        self.move_label = self.game_control.move_label
    def reset_ui(self):
        # Destroy old frame
        self.frame.destroy()
        
        # Tạo lại frame mới
        self.frame = tk.Frame(self.master)
        self.frame.pack(fill="both", expand=True)
        
        # Reset board state
        self.board_state = self.init_board()
        self.selected = None
        self.last_move = None
        self.is_my_turn = (self.my_color == "white")
        
        # Tạo UI lại
        self.setup_ui()
        self.draw_board()

    def draw_board(self):
        self.canvas.delete("all")
        cell_size = 75
        
        # Draw squares - board orientation based on player color
        # board_state[0] = rank 8, board_state[7] = rank 1
        # WHITE player: display bottom = rank 1 (board_state[7])
        # BLACK player: display bottom = rank 8 (board_state[0])
        
        for display_row in range(8):
            for display_col in range(8):
                # WHITE sees: top=rank8(row0), bottom=rank1(row7)
                # BLACK sees: top=rank1(row7), bottom=rank8(row0)
                if self.my_color == "white":
                    board_row = display_row  # top=0=rank8, bottom=7=rank1
                else:
                    board_row = 7 - display_row  # top=0→7=rank1, bottom=7→0=rank8
                
                board_col = display_col  # Same for both
                
                x1 = display_col * cell_size
                y1 = display_row * cell_size
                x2 = x1 + cell_size
                y2 = y1 + cell_size
                
                color = "#f0d9b5" if (board_row + board_col) % 2 == 0 else "#b58863"
                self.canvas.create_rectangle(x1, y1, x2, y2, fill=color, outline="")
                
                # Highlight last move squares
                if self.last_move:
                    try:
                        from_square = self.last_move[:2]
                        to_square = self.last_move[2:4]
                        from_row, from_col = self.square_to_coords(from_square)
                        to_row, to_col = self.square_to_coords(to_square)
                        if board_row == from_row and board_col == from_col:
                            self.canvas.create_rectangle(x1, y1, x2, y2, fill="#ffff99", outline="")  # Light yellow for from
                        elif board_row == to_row and board_col == to_col:
                            self.canvas.create_rectangle(x1, y1, x2, y2, fill="#ffcc99", outline="")  # Light orange for to
                    except:
                        pass
                
                # Highlight selected square
                if self.selected and self.selected == (board_row, board_col):
                    self.canvas.create_rectangle(x1, y1, x2, y2, fill="#7fc97f", outline="")
                
                # Highlight valid moves (show where selected piece can go)
                if self.selected:
                    from_row, from_col = self.selected
                    if self._is_valid_target(from_row, from_col, board_row, board_col):
                        # Draw a circle or highlight
                        center_x = x1 + cell_size // 2
                        center_y = y1 + cell_size // 2
                        radius = 10
                        self.canvas.create_oval(
                            center_x - radius, center_y - radius,
                            center_x + radius, center_y + radius,
                            fill="#90EE90", outline="#228B22", width=2
                        )
                
                # Draw piece
                piece = self.board_state[board_row][board_col]
                if piece:
                    self.canvas.create_text(x1 + cell_size//2, y1 + cell_size//2,
                                          text=self.get_piece_symbol(piece),
                                          font=("Arial", 40), fill="black")
    
    def _is_valid_target(self, from_row, from_col, to_row, to_col):
        """Check if a move from (from_row, from_col) to (to_row, to_col) might be valid.
        This is a simple heuristic to show possible moves, not full chess validation."""
        
        # Can't move to same square
        if from_row == to_row and from_col == to_col:
            return False
        
        piece = self.board_state[from_row][from_col]
        target = self.board_state[to_row][to_col]
        
        # Can't capture own piece
        if target:
            if (piece.isupper() and target.isupper()) or (piece.islower() and target.islower()):
                return False
        
        piece_type = piece.upper()
        
        # Simple movement rules (not comprehensive, just for hints)
        if piece_type == 'P':  # Pawn
            direction = -1 if piece.isupper() else 1  # White moves up, Black moves down
            # Forward one square
            if to_col == from_col and to_row == from_row + direction and not target:
                return True
            # Forward two squares from start
            start_row = 6 if piece.isupper() else 1
            if from_row == start_row and to_col == from_col and to_row == from_row + 2 * direction and not target:
                return True
            # Capture diagonally
            if abs(to_col - from_col) == 1 and to_row == from_row + direction and target:
                return True
            return False
        
        elif piece_type == 'N':  # Knight
            return (abs(to_row - from_row), abs(to_col - from_col)) in [(2,1), (1,2)]
        
        elif piece_type == 'B':  # Bishop
            return abs(to_row - from_row) == abs(to_col - from_col) and self._is_path_clear(from_row, from_col, to_row, to_col)
        
        elif piece_type == 'R':  # Rook
            return (to_row == from_row or to_col == from_col) and self._is_path_clear(from_row, from_col, to_row, to_col)
        
        elif piece_type == 'Q':  # Queen
            return ((to_row == from_row or to_col == from_col) or 
                    abs(to_row - from_row) == abs(to_col - from_col)) and self._is_path_clear(from_row, from_col, to_row, to_col)
        
        elif piece_type == 'K':  # King
            return abs(to_row - from_row) <= 1 and abs(to_col - from_col) <= 1
        
        return False
    
    def _is_path_clear(self, from_row, from_col, to_row, to_col):
        """Check if path between two squares is clear (for sliding pieces)"""
        row_dir = 0 if to_row == from_row else (1 if to_row > from_row else -1)
        col_dir = 0 if to_col == from_col else (1 if to_col > from_col else -1)
        
        current_row, current_col = from_row + row_dir, from_col + col_dir
        
        while (current_row, current_col) != (to_row, to_col):
            if self.board_state[current_row][current_col]:
                return False
            current_row += row_dir
            current_col += col_dir
        
        return True
    
    def get_piece_symbol(self, piece):
        symbols = {
            'K': '♔', 'Q': '♕', 'R': '♖', 'B': '♗', 'N': '♘', 'P': '♙',
            'k': '♚', 'q': '♛', 'r': '♜', 'b': '♝', 'n': '♞', 'p': '♟'
        }
        return symbols.get(piece, piece)
    
    def on_click(self, event):
        if not self.is_my_turn:
            self.status_label.config(text="Not your turn!", fg="red")
            return
        
        cell_size = 75
        display_col = event.x // cell_size
        display_row = event.y // cell_size
        
        # Convert display coordinates to board coordinates
        # MUST match the logic in draw_board()!
        if self.my_color == "white":
            board_row = display_row  # Direct mapping for white
        else:
            board_row = 7 - display_row  # Flipped for black
        
        board_col = display_col
        
        if board_row < 0 or board_row >= 8 or board_col < 0 or board_col >= 8:
            return
        
        if not self.selected:
            # Select piece
            piece = self.board_state[board_row][board_col]
            if piece:
                # Check if it's my piece
                if (self.my_color == "white" and piece.isupper()) or \
                   (self.my_color == "black" and piece.islower()):
                    self.selected = (board_row, board_col)
                    self.draw_board()
        else:
            # Make move
            from_row, from_col = self.selected
            to_row, to_col = board_row, board_col
            
            # Convert board coordinates to chess notation
            # board_row: 0=rank8, 1=rank7, ..., 7=rank1
            # board_col: 0=a, 1=b, ..., 7=h
            from_square = f"{chr(from_col + ord('a'))}{8 - from_row}"
            to_square = f"{chr(to_col + ord('a'))}{8 - to_row}"
            
            # Debug output
            piece = self.board_state[from_row][from_col]
            print(f"[PvP {self.my_color.upper()}] Clicked: display_row={display_row}, display_col={display_col}")
            print(f"[PvP {self.my_color.upper()}] Board: from=({from_row},{from_col})[{piece}] to=({to_row},{to_col})")
            print(f"[PvP {self.my_color.upper()}] Notation: {from_square} \u2192 {to_square}")
            
            # Send move to server (send() will add newline)
            self.client.send(f"MOVE|{self.match_id}|{self.user_id}|{from_square}|{to_square}")
            print(f"[PvP] Sending move: MOVE|{self.match_id}|{self.user_id}|{from_square}|{to_square}")
            
            # Don't update board yet - wait for server confirmation
            self.status_label.config(text="Sending move...", fg="orange")
            
            self.selected = None
            self.draw_board()
    
    def surrender(self):
        if messagebox.askyesno("Surrender", "Are you sure you want to surrender?"):
            self.client.send(f"SURRENDER|{self.match_id}|{self.user_id}")
            self.status_label.config(text="You surrendered", fg="red")
    
    def offer_draw(self):
        self.client.send(f"DRAW|{self.match_id}|{self.user_id}")
        self.status_label.config(text="Draw offer sent")
    
    def back_to_menu(self):
        # If game is not over, send surrender before leaving
        if self.game_control.game_state != GameControlUI.STATE_GAME_OVER:
            self.client.send(f"SURRENDER|{self.match_id}|{self.user_id}\n")
        self.frame.destroy()
        self.on_back()
    

    def handle_message(self, msg):
        if not self.frame.winfo_exists():
            return

        msg = msg.strip()
        print(f"[PvP] Received: {msg}")

        # ================= CHAT MESSAGES =================
        if msg.startswith("GAME_CHAT_FROM"):
            parts = msg.split('|', 2)
            if len(parts) >= 3:
                sender_name = parts[1]
                chat_message = parts[2]
                if self.game_chat and self.game_chat.winfo_exists():
                    self.game_chat.add_message(sender_name, chat_message, is_self=False)
            return

        # ================= ERROR =================
        if msg.startswith("ERROR"):
            err = msg.split('|', 1)[1] if '|' in msg else msg
            if self.status_label.winfo_exists():
                self.status_label.config(text=err, fg="red")
            return

        # ================= MOVES =================
        if msg.startswith("MOVE_SUCCESS"):
            _, move, fen = msg.split('|', 2)
            self.last_move = move
            self.move_label.config(text=f"Your move: {move}")
            self.update_board_from_fen(fen)
            self.is_my_turn = False
            self.status_label.config(text="Opponent's turn", fg="#2b2b2b")
            self.draw_board()
            return

        if msg.startswith("OPPONENT_MOVE"):
            _, move, fen = msg.split('|', 2)
            self.last_move = move
            self.move_label.config(text=f"Opponent: {move}")
            self.update_board_from_fen(fen)
            self.is_my_turn = True
            self.status_label.config(text="Your turn", fg="green")
            self.draw_board()
            return

        # ================= PAUSE / RESUME =================
        if msg == "GAME_PAUSED_BY_OPPONENT":
            self.game_control.game_state = GameControlUI.STATE_PAUSED_BY_OPPONENT
            self.game_control.status_label.config(
                text="Opponent paused the game", fg="orange"
            )
            self.game_control.update_ui_by_state()
            return

        if msg == "GAME_RESUMED":
            self.game_control.game_state = GameControlUI.STATE_NORMAL
            self.game_control.status_label.config(
                text="Game resumed", fg="green"
            )
            self.game_control.update_ui_by_state()
            return

        # ================= DRAW =================
        if msg.startswith("DRAW_REQUEST_FROM_OPPONENT"):
            self.game_control.game_state = GameControlUI.STATE_DRAW_OFFER_RECEIVED
            self.game_control.status_label.config(
                text="Opponent offered a draw", fg="blue"
            )
            self.game_control.update_ui_by_state()
            return

        if msg == "DRAW_ACCEPTED":
            self.is_my_turn = False
            self.game_control.game_state = GameControlUI.STATE_GAME_OVER
            self.game_control.status_label.config(
                text="Game ended in a draw", fg="blue"
            )
            self.game_control.update_ui_by_state()
            try:
                self.client.send(f"GET_ELO_HISTORY|{self.user_id}\n")
                self._pending_result_text = "Game ended in a draw"
            except Exception:
                show_toast(self.master, "Game ended in a draw", kind="info")
            return

        if msg.startswith("DRAW_DECLINED_BY_OPPONENT"):
            # Người xin hòa nhận thông báo bị từ chối
            self.game_control.game_state = GameControlUI.STATE_NORMAL
            self.game_control.status_label.config(
                text="Your draw offer was declined", fg="orange"
            )
            self.game_control.update_ui_by_state()
            return


        
        if msg == "DRAW_DECLINED":
            self.game_control.game_state = GameControlUI.STATE_NORMAL
            self.game_control.status_label.config(
                text="You declined draw", fg="orange"
            )
            self.game_control.update_ui_by_state()


        # ================= SURRENDER / END =================
        if msg == "SURRENDER_SUCCESS":
            self.is_my_turn = False
            self.game_control.game_state = GameControlUI.STATE_GAME_OVER
            self.game_control.status_label.config(
                text="You surrendered", fg="red"
            )
            self.game_control.update_ui_by_state()
            return

        if msg.startswith("GAME_END"):
            self.stop_timer()
            parts = msg.split('|')
            reason = parts[1]
            winner = parts[2] if len(parts) > 2 else ""

            if "winner:" in winner and int(winner.split(':')[1]) == self.user_id:
                text, color = "You won!", "green"
            else:
                text, color = "You lost!", "red"

            self.is_my_turn = False
            self.game_control.game_state = GameControlUI.STATE_GAME_OVER
            self.game_control.status_label.config(
                text=f"{text} ({reason})", fg=color
            )
            self.game_control.update_ui_by_state()
            # Request ELO history to show delta
            try:
                self.client.send(f"GET_ELO_HISTORY|{self.user_id}\n")
                # Temporarily stash result text; overlay will be shown when ELO_HISTORY arrives
                self._pending_result_text = text
            except Exception:
                show_toast(self.master, text, kind=("success" if color == "green" else "error"))
            return

        # ================= OPPONENT DISCONNECTED =================
        if msg.startswith("OPPONENT_DISCONNECTED"):
            self.stop_timer()
            self.is_my_turn = False
            self.game_control.game_state = GameControlUI.STATE_GAME_OVER
            self.game_control.status_label.config(
                text="You won! (Opponent disconnected)", fg="green"
            )
            self.game_control.update_ui_by_state()
            try:
                self.client.send(f"GET_ELO_HISTORY|{self.user_id}\n")
                self._pending_result_text = "You won! (Opponent disconnected)"
            except Exception:
                show_toast(self.master, "You won! (Opponent disconnected)", kind="success")
            return

        # ================= TIMER =================
        if msg.startswith("TIMER_UPDATE"):
            _, white_time, black_time = msg.split('|')
            self.white_timer_label.config(text=f"White: {self.format_time(int(white_time))}")
            self.black_timer_label.config(text=f"Black: {self.format_time(int(black_time))}")
            return

        # ================= ELO HISTORY RESPONSE =================
        if msg.startswith("ELO_HISTORY|"):
            parts = msg.strip().split('|')
            try:
                count = int(parts[1]) if len(parts) > 1 else 0
            except ValueError:
                count = 0
            if count >= 1 and len(parts) >= 7:
                # Latest entry: match_id, old_elo, new_elo, elo_change, date
                old_elo = int(parts[3])
                new_elo = int(parts[4])
                elo_change = int(parts[5])
                result_text = getattr(self, "_pending_result_text", "Kết thúc trận")
                show_result_overlay(self.master, result_text, new_elo, elo_change)
                # Clear pending
                self._pending_result_text = None
            else:
                # No history yet
                result_text = getattr(self, "_pending_result_text", "Kết thúc trận")
                show_result_overlay(self.master, result_text, None, None)
                self._pending_result_text = None
            return

        # ================= REMATCH =================
        if msg == "REMATCH_REQUESTED":
            # server ACK request → ignore UI change
            return

        if msg == "OPPONENT_REMATCH_REQUEST":
            self.game_control.game_state = GameControlUI.STATE_REMATCH_RECEIVED
            self.game_control.status_label.config(
                text="Opponent wants a rematch", fg="purple"
            )
            self.game_control.update_ui_by_state()
            return

        if msg == "REMATCH_DECLINED":
            self.game_control.game_state = GameControlUI.STATE_GAME_OVER
            self.game_control.status_label.config(
                text="Rematch declined", fg="red"
            )
            self.game_control.update_ui_by_state()
            return
        if msg.startswith("REMATCH_DECLINED_BY_OPPONENT"):
            self.game_control.game_state = GameControlUI.STATE_GAME_OVER
            self.game_control.status_label.config(
                text="Your rematch request was declined", fg="purple"
            )
            self.game_control.update_ui_by_state()
            return


        if msg.startswith("REMATCH_START"):
            _, new_match_id, color = msg.split('|')

            self.match_id = int(new_match_id)
            self.my_color = color

            # Reset UI hoàn toàn
            self.reset_ui()  # reset_ui sẽ tạo lại frame, canvas, game_control

            # Sau reset_ui, các thuộc tính được khởi tạo mới
            self.is_my_turn = (self.my_color == "white")
            self.game_control.match_id = self.match_id
            self.game_control.my_color = self.my_color
            self.game_control.game_state = GameControlUI.STATE_NORMAL
            self.game_control.status_label.config(text="New game started", fg="green")
            self.game_control.update_ui_by_state()

    def update_board_from_fen(self, fen):
        """Update board state from FEN string"""
        parts = fen.split()
        if not parts:
            return
        
        board_fen = parts[0]
        rows = board_fen.split('/')
        
        self.board_state = [['' for _ in range(8)] for _ in range(8)]
        
        for row_idx, row_str in enumerate(rows):
            col_idx = 0
            for char in row_str:
                if char.isdigit():
                    col_idx += int(char)
                else:
                    if col_idx < 8:
                        self.board_state[row_idx][col_idx] = char
                        col_idx += 1
        
    def format_time(self, seconds):
        """Format seconds to MM:SS"""
        minutes = seconds // 60
        secs = seconds % 60
        return f"{minutes:02d}:{secs:02d}"
    
    def update_timer_display(self):
        """Update timer labels"""
        try:
            if self.white_timer_label.winfo_exists() and self.black_timer_label.winfo_exists():
                self.white_timer_label.config(text=f"White: {self.format_time(self.white_time)}")
                self.black_timer_label.config(text=f"Black: {self.format_time(self.black_time)}")
        except tk.TclError:
            # Widget has been destroyed, stop the timer
            self.stop_timer()
    
    def start_timer(self):
        """Start the timer countdown"""
        if self.timer_running:
            return
        self.timer_running = True
        self.update_timer()
    
    def stop_timer(self):
        """Stop the timer"""
        self.timer_running = False
        if self.timer_id:
            self.master.after_cancel(self.timer_id)
            self.timer_id = None
    
    def update_timer(self):
        """Update timer every second"""
        if not self.timer_running:
            return
        
        if self.is_my_turn:
            # Opponent's turn, deduct from opponent's time
            if self.my_color == "white":
                self.black_time -= 1
                if self.black_time <= 0:
                    self.black_time = 0
                    self.game_over_timeout("Black")
                    return
            else:
                self.white_time -= 1
                if self.white_time <= 0:
                    self.white_time = 0
                    self.game_over_timeout("White")
                    return
        else:
            # My turn, deduct from my time
            if self.my_color == "white":
                self.white_time -= 1
                if self.white_time <= 0:
                    self.white_time = 0
                    self.game_over_timeout("White")
                    return
            else:
                self.black_time -= 1
                if self.black_time <= 0:
                    self.black_time = 0
                    self.game_over_timeout("Black")
                    return
        
        self.update_timer_display()
        self.timer_id = self.master.after(1000, self.update_timer)
    
    def game_over_timeout(self, loser_color):
        """Handle game over by timeout"""
        self.stop_timer()
        winner = "White" if loser_color == "Black" else "Black"
        self.game_control.status_label.config(text=f"{loser_color} timeout! {winner} wins", fg="red")
        self.game_control.game_state = GameControlUI.STATE_GAME_OVER
        self.game_control.update_ui_by_state()

    def format_time(self, seconds):
        """Format seconds to MM:SS"""
        minutes = seconds // 60
        secs = seconds % 60
        return f"{minutes:02d}:{secs:02d}"

    def square_to_coords(self, square):
        """Convert chess square (e.g., 'e4') to board coordinates (row, col)"""
        col = ord(square[0]) - ord('a')
        row = 8 - int(square[1])
        return row, col
