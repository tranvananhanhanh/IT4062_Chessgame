import tkinter as tk
from tkinter import messagebox, ttk
from ui.notifications import show_toast

class HistoryUI:
    """UI for viewing game history and replays via C Server socket"""
    
    def __init__(self, master, user_id, client, on_back):
        self.master = master
        self.user_id = user_id
        self.client = client  # PollClient for socket communication
        self.on_back = on_back
        self.waiting_for_response = False
        self.response_type = None
        
        self.frame = tk.Frame(master, bg="#f0f0f0")
        self.frame.pack(fill="both", expand=True)
        
        self.history_data = []
        self.setup_ui()
        self.load_history()
    
    def setup_ui(self):
        """Setup the UI components"""
        # Title
        title_frame = tk.Frame(self.frame, bg="#2b2b2b", height=60)
        title_frame.pack(side="top", fill="x")
        title_frame.pack_propagate(False)
        
        tk.Label(title_frame, text="Game History", 
                font=("Helvetica", 16, "bold"), bg="#2b2b2b", fg="white").pack(pady=15)
        
        # Main container
        container = tk.Frame(self.frame, bg="#f0f0f0")
        container.pack(fill="both", expand=True, padx=20, pady=20)
        
        # History list with scrollbar
        list_frame = tk.Frame(container, bg="#ffffff", relief="ridge", bd=2)
        list_frame.pack(fill="both", expand=True, pady=(0, 10))
        
        # Create Treeview for history
        columns = ("Match ID", "Opponent", "Result", "Color", "Moves", "Date")
        self.tree = ttk.Treeview(list_frame, columns=columns, show="headings", height=15)
        
        # Define column headings
        for col in columns:
            self.tree.heading(col, text=col)
        
        # Define column widths
        self.tree.column("Match ID", width=80, anchor="center")
        self.tree.column("Opponent", width=150, anchor="w")
        self.tree.column("Result", width=80, anchor="center")
        self.tree.column("Color", width=80, anchor="center")
        self.tree.column("Moves", width=80, anchor="center")
        self.tree.column("Date", width=180, anchor="w")
        
        # Add scrollbar
        scrollbar = ttk.Scrollbar(list_frame, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=scrollbar.set)
        
        self.tree.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        
        # Bind double-click event for replay
        self.tree.bind("<Double-1>", self.on_match_double_click)
        
        # Button frame
        button_frame = tk.Frame(container, bg="#f0f0f0")
        button_frame.pack(fill="x")
        
        tk.Button(button_frame, text="View Replay", font=("Helvetica", 11, "bold"),
                 width=15, command=self.view_selected_replay).pack(side="left", padx=5)
        tk.Button(button_frame, text="Refresh", font=("Helvetica", 11, "bold"),
                 width=15, command=self.load_history).pack(side="left", padx=5)
        tk.Button(button_frame, text="Back to Menu", font=("Helvetica", 11, "bold"),
                 width=15, command=self.back_to_menu).pack(side="right", padx=5)
        
        # Status label
        self.status_label = tk.Label(container, text="Loading history...", 
                                     font=("Helvetica", 10), bg="#f0f0f0", fg="#666")
        self.status_label.pack(pady=(10, 0))
    
    def load_history(self):
        """Load game history from C Server via socket"""
        try:
            self.status_label.config(text="Loading history...", fg="#666")
            
            # Send GET_HISTORY command to C Server
            self.client.send(f"GET_HISTORY|{self.user_id}")
            self.waiting_for_response = True
            self.response_type = "history"
            
        except Exception as e:
            self.status_label.config(text=f"Error: {str(e)}", fg="red")
            messagebox.showerror("Error", f"Failed to request history:\n{str(e)}")
    
    def handle_message(self, msg):
        """Handle incoming messages from server"""
        if msg.startswith("HISTORY|"):
            # Parse history response
            try:
                import json
                history_json = msg.split("|", 1)[1]
                history_obj = json.loads(history_json)
                self.history_data = history_obj.get("matches", [])
                self.populate_history()
                self.status_label.config(text=f"Loaded {len(self.history_data)} matches", fg="green")
                show_toast(self.master, f"✓ Đã tải {len(self.history_data)} trận đấu", kind="success")
                self.waiting_for_response = False
            except Exception as e:
                show_toast(self.master, f"✗ Lỗi phân tích dữ liệu: {str(e)}", kind="error")
                self.waiting_for_response = False
        elif msg.startswith("REPLAY|") and self.response_type == "replay":
            # Parse replay response
            try:
                import json
                replay_json = msg.split("|", 1)[1]
                replay_data = json.loads(replay_json)
                self.open_replay_window(self._replay_match_id, replay_data)
                self.waiting_for_response = False
                self.response_type = None
            except Exception as e:
                show_toast(self.master, f"✗ Lỗi phân tích replay: {str(e)}", kind="error")
                self.waiting_for_response = False
                self.response_type = None
        elif msg.startswith("ERROR") and self.waiting_for_response:
            # Parse error message
            error_msg = msg.split("|", 1)[1] if "|" in msg else msg
            if "no moves" in error_msg.lower():
                show_toast(self.master, "⚠ Trận này không có nước đi (surrender ngay)", kind="warning")
            elif "not found" in error_msg.lower():
                show_toast(self.master, "✗ Không tìm thấy trận đấu", kind="error")
            else:
                show_toast(self.master, f"✗ {error_msg}", kind="error")
            self.waiting_for_response = False
            self.response_type = None
    
    def _check_history_response(self):
        """Check for history response from server"""
        if not self.waiting_for_response:
            return
        
        responses = self.client.poll()
        for resp in responses:
            if resp.startswith("HISTORY|"):
                # Parse history response
                try:
                    import json
                    history_json = resp.split("|", 1)[1]
                    history_obj = json.loads(history_json)
                    self.history_data = history_obj.get("matches", [])
                    self.populate_history()
                    self.status_label.config(text=f"Loaded {len(self.history_data)} matches", fg="green")
                    self.waiting_for_response = False
                    return
                except Exception as e:
                    self.status_label.config(text=f"Parse error: {str(e)}", fg="red")
                    self.waiting_for_response = False
                    return
            elif resp.startswith("ERROR"):
                self.status_label.config(text=resp, fg="red")
                self.waiting_for_response = False
                return
        
        # Keep checking
        if self.waiting_for_response:
            self.master.after(50, self._check_history_response)
    
    def populate_history(self):
        """Populate the treeview with history data"""
        # Clear existing items
        for item in self.tree.get_children():
            self.tree.delete(item)
        
        # Add history items
        for match in self.history_data:
            match_id = match.get("match_id", "?")
            opponent = match.get("opponent_name", "Unknown")
            result = match.get("result", "?").upper()
            color = match.get("player_color", "?").capitalize()
            moves = match.get("move_count", 0)
            date = match.get("start_time", "?")
            
            # Color code the result
            tag = ""
            if result == "WIN":
                tag = "win"
            elif result == "LOSS":
                tag = "loss"
            elif result == "DRAW":
                tag = "draw"
            
            self.tree.insert("", "end", values=(match_id, opponent, result, color, moves, date), tags=(tag,))
        
        # Configure tags for color coding - green for wins, red for losses
        self.tree.tag_configure("win", background="#90EE90", foreground="#006400")  # Light green bg, dark green text
        self.tree.tag_configure("loss", background="#FFB6C1", foreground="#8B0000")  # Light red bg, dark red text
        self.tree.tag_configure("draw", background="#FFE4B5", foreground="#8B4513")  # Light orange bg, brown text
    
    def on_match_double_click(self, event):
        """Handle double-click on a match"""
        self.view_selected_replay()
    
    def view_selected_replay(self):
        """View replay of selected match"""
        selection = self.tree.selection()
        if not selection:
            messagebox.showwarning("No Selection", "Please select a match to view replay")
            return
        
        item = self.tree.item(selection[0])
        match_id = item["values"][0]
        
        self.show_replay(match_id)
    
    def show_replay(self, match_id):
        """Show replay window for a match"""
        try:
            # Send GET_REPLAY command to C Server
            self.client.send(f"GET_REPLAY|{match_id}")
            self.waiting_for_response = True
            self.response_type = "replay"
            self._replay_match_id = match_id
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to request replay:\n{str(e)}")
    
    def _check_replay_response(self):
        """Check for replay response from server"""
        if not self.waiting_for_response or self.response_type != "replay":
            return
        
        responses = self.client.poll()
        for resp in responses:
            if resp.startswith("REPLAY|"):
                # Parse replay response
                try:
                    import json
                    replay_json = resp.split("|", 1)[1]
                    replay_data = json.loads(replay_json)
                    self.open_replay_window(self._replay_match_id, replay_data)
                    self.waiting_for_response = False
                    return
                except Exception as e:
                    messagebox.showerror("Error", f"Parse error:\n{str(e)}")
                    self.waiting_for_response = False
                    return
            elif resp.startswith("ERROR"):
                messagebox.showerror("Error", f"Failed to load replay:\n{resp}")
                self.waiting_for_response = False
                return
        
        # Keep checking
        if self.waiting_for_response:
            self.master.after(50, self._check_replay_response)
    
    def open_replay_window(self, match_id, replay_data):
        """Open a new window to show the replay"""
        replay_window = tk.Toplevel(self.master)
        replay_window.title(f"Replay - Match {match_id}")
        replay_window.geometry("900x800")
        replay_window.configure(bg="#f0f0f0")
        
        ReplayViewer(replay_window, match_id, replay_data)
    
    def back_to_menu(self):
        """Return to main menu"""
        self.frame.destroy()
        self.on_back()


class ReplayViewer:
    """Viewer for replaying a match move by move"""
    
    def __init__(self, master, match_id, replay_data):
        self.master = master
        self.match_id = match_id
        self.moves = replay_data.get("moves", [])
        self.current_move = 0
        
        self.setup_ui()
        self.show_position()
    
    def setup_ui(self):
        """Setup replay viewer UI"""
        # Title
        tk.Label(self.master, text=f"Match {self.match_id} Replay", 
                font=("Helvetica", 16, "bold"), bg="#f0f0f0").pack(pady=10)
        
        # Board canvas
        self.canvas = tk.Canvas(self.master, width=600, height=600, bg="#ffffff")
        self.canvas.pack(pady=10)
        
        # Move info
        self.move_label = tk.Label(self.master, text="Move: 0 / 0", 
                                   font=("Helvetica", 12), bg="#f0f0f0")
        self.move_label.pack(pady=5)
        
        self.notation_label = tk.Label(self.master, text="Notation: -", 
                                       font=("Helvetica", 11), bg="#f0f0f0")
        self.notation_label.pack(pady=5)
        
        # Control buttons
        button_frame = tk.Frame(self.master, bg="#f0f0f0")
        button_frame.pack(pady=10)
        
        tk.Button(button_frame, text="◀", font=("Helvetica", 14, "bold"),
                 width=5, command=self.previous_move).pack(side="left", padx=10)
        tk.Button(button_frame, text="▶", font=("Helvetica", 14, "bold"),
                 width=5, command=self.next_move).pack(side="left", padx=10)
    
    def show_position(self):
        """Display current position"""
        if self.current_move == 0:
            # Initial position
            fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
            notation = "Starting position"
        elif self.current_move <= len(self.moves):
            move = self.moves[self.current_move - 1]
            fen = move.get("fen_after", "")
            notation = move.get("notation", "?")
        else:
            return
        
        self.draw_board_from_fen(fen)
        self.move_label.config(text=f"Move: {self.current_move} / {len(self.moves)}")
        self.notation_label.config(text=f"Notation: {notation}")
    
    def draw_board_from_fen(self, fen):
        """Draw chess board from FEN string"""
        self.canvas.delete("all")
        
        if not fen:
            return
        
        # Parse FEN
        parts = fen.split()
        if not parts:
            return
        
        board_fen = parts[0]
        rows = board_fen.split('/')
        
        cell_size = 75
        
        # Draw board
        for display_row in range(8):
            for display_col in range(8):
                board_row = display_row
                board_col = display_col
                
                x1 = display_col * cell_size
                y1 = display_row * cell_size
                x2 = x1 + cell_size
                y2 = y1 + cell_size
                
                # Alternating colors
                color = "#f0d9b5" if (board_row + board_col) % 2 == 0 else "#b58863"
                self.canvas.create_rectangle(x1, y1, x2, y2, fill=color, outline="")
                
                # Draw piece if present
                if board_row < len(rows):
                    row_str = rows[board_row]
                    col_idx = 0
                    for char in row_str:
                        if char.isdigit():
                            col_idx += int(char)
                        else:
                            if col_idx == board_col:
                                piece_symbol = self.get_piece_symbol(char)
                                self.canvas.create_text(x1 + cell_size//2, y1 + cell_size//2,
                                                       text=piece_symbol,
                                                       font=("Arial", 40), fill="black")
                                break
                            col_idx += 1
    
    def get_piece_symbol(self, piece):
        """Get Unicode symbol for chess piece"""
        symbols = {
            'K': '♔', 'Q': '♕', 'R': '♖', 'B': '♗', 'N': '♘', 'P': '♙',
            'k': '♚', 'q': '♛', 'r': '♜', 'b': '♝', 'n': '♞', 'p': '♟'
        }
        return symbols.get(piece, piece)
    
    def first_move(self):
        """Go to first move (starting position)"""
        self.current_move = 0
        self.show_position()
    
    def previous_move(self):
        """Go to previous move"""
        if self.current_move > 0:
            self.current_move -= 1
            self.show_position()
    
    def next_move(self):
        """Go to next move"""
        if self.current_move < len(self.moves):
            self.current_move += 1
            self.show_position()
    
    def last_move(self):
        """Go to last move"""
        self.current_move = len(self.moves)
        self.show_position()
