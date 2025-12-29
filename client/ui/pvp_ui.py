import tkinter as tk
from tkinter import messagebox

class PvPUI:
    def __init__(self, master, match_id, my_color, opponent_name, client, on_back, user_id):
        self.master = master
        self.match_id = match_id
        self.my_color = my_color
        self.opponent_name = opponent_name
        self.client = client
        self.on_back = on_back
        self.user_id = user_id
        
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
        
        # Main container
        container = tk.Frame(self.frame, bg="#f0f0f0")
        container.pack(fill="both", expand=True)
        
        # Left panel - Board
        left_panel = tk.Frame(container, bg="#f0f0f0")
        left_panel.pack(side="left", fill="both", expand=True, padx=20, pady=20)
        
        self.canvas = tk.Canvas(left_panel, width=600, height=600, bg="#ffffff")
        self.canvas.pack()
        self.canvas.bind("<Button-1>", self.on_click)
        
        # Right panel - Controls
        right_panel = tk.Frame(container, bg="#f0f0f0", width=250)
        right_panel.pack(side="right", fill="y", padx=(0, 20), pady=20)
        right_panel.pack_propagate(False)
        
        tk.Label(right_panel, text="Game Controls", 
                font=("Helvetica", 14, "bold"), bg="#f0f0f0").pack(pady=(0, 20))
        
        self.status_label = tk.Label(right_panel, text="Your turn" if self.my_color == "white" else "Opponent's turn",
                                     font=("Helvetica", 11), bg="#f0f0f0", fg="#2b2b2b", wraplength=220)
        self.status_label.pack(pady=10)
        
        self.move_label = tk.Label(right_panel, text="Last move: -",
                                   font=("Helvetica", 10), bg="#f0f0f0", wraplength=220)
        self.move_label.pack(pady=10)
        
        tk.Button(right_panel, text="Surrender", font=("Helvetica", 11, "bold"),
                 width=18, command=self.surrender).pack(pady=10)
        tk.Button(right_panel, text="Offer Draw", font=("Helvetica", 11, "bold"),
                 width=18, command=self.offer_draw).pack(pady=10)
        tk.Button(right_panel, text="Back to Menu", font=("Helvetica", 11, "bold"),
                 width=18, command=self.back_to_menu).pack(pady=(40, 0))
    
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
        self.frame.destroy()
        self.on_back()
    
    def handle_message(self, msg):
        """Handle server messages"""
        msg = msg.strip()
        print(f"[PvP] Received: {msg}")
        
        if msg.startswith("MOVE_SUCCESS"):
            # MOVE_SUCCESS|move|fen
            parts = msg.split('|')
            if len(parts) >= 3:
                move = parts[1]
                fen = parts[2]
                self.last_move = move
                self.move_label.config(text=f"Your move: {move}")
                self.update_board_from_fen(fen)
                # Đổi lượt
                self.is_my_turn = False
                self.status_label.config(text="Opponent's turn", fg="#2b2b2b")
                self.draw_board()  # Redraw to show updated position
                self.canvas.update_idletasks()  # Force canvas refresh
        elif msg.startswith("OPPONENT_MOVE"):
            # OPPONENT_MOVE|move|fen
            parts = msg.split('|')
            if len(parts) >= 3:
                move = parts[1]
                fen = parts[2]
                self.last_move = move
                self.move_label.config(text=f"Opponent: {move}")
                self.update_board_from_fen(fen)
                self.is_my_turn = True
                self.status_label.config(text="Your turn", fg="green")
                self.draw_board()
                self.canvas.update_idletasks()  # Force canvas refresh
        elif msg.startswith("GAME_END"):
            # GAME_END|surrender|winner:X or GAME_END|checkmate|winner:X
            parts = msg.split('|')
            if len(parts) >= 2:
                reason = parts[1]  # surrender, checkmate, stalemate, draw
                winner_info = parts[2] if len(parts) >= 3 else ""
                
                # Determine if we won or lost
                if "winner:" in winner_info:
                    winner_id = int(winner_info.split(':')[1])
                    if winner_id == self.user_id:
                        result_text = f"You Won! ({reason})"
                        color = "green"
                    else:
                        result_text = f"You Lost ({reason})"
                        color = "red"
                else:
                    result_text = f"Game Ended: {reason}"
                    color = "orange"
                
                self.is_my_turn = False
                self.status_label.config(text=result_text, fg=color)
                messagebox.showinfo("Game Over", result_text)
        elif msg.startswith("SURRENDER_SUCCESS"):
            # SURRENDER_SUCCESS|winner_id
            parts = msg.split('|')
            if len(parts) >= 2:
                winner_id = int(parts[1])
                if winner_id == self.user_id:
                    self.status_label.config(text="Opponent surrendered - You won!", fg="green")
                else:
                    self.status_label.config(text="You surrendered", fg="red")
        elif msg.startswith("OPPONENT_DISCONNECTED"):
            try:
                if self.status_label.winfo_exists():
                    self.status_label.config(text="Opponent disconnected", fg="orange")
                    messagebox.showinfo("Game Info", "Opponent has disconnected from the game")
            except:
                pass  # Widget already destroyed
        elif msg.startswith("GAME_OVER"):
            # GAME_OVER|match_id|result
            parts = msg.split('|')
            if len(parts) >= 3:
                result = parts[2]
                self.status_label.config(text=f"Game Over: {result}", fg="red")
                messagebox.showinfo("Game Over", f"Result: {result}")
        elif msg.startswith("ERROR"):
            # Handle all ERROR messages
            print(f"[PvP ERROR] {msg}")
            self.status_label.config(text=msg, fg="red")
            # Reset turn state if move was rejected
            if "turn" in msg.lower():
                self.is_my_turn = False
    
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
