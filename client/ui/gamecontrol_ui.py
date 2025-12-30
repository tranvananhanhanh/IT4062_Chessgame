import tkinter as tk

class GameControlUI(tk.Frame):
    # Game state constants
    STATE_NORMAL = "NORMAL"
    STATE_PAUSED_BY_ME = "PAUSED_BY_ME"
    STATE_PAUSED_BY_OPPONENT = "PAUSED_BY_OPPONENT"
    STATE_DRAW_OFFER_SENT = "DRAW_OFFER_SENT"
    STATE_DRAW_OFFER_RECEIVED = "DRAW_OFFER_RECEIVED"
    STATE_GAME_OVER = "GAME_OVER"
    STATE_REMATCH_SENT = "REMATCH_SENT"
    STATE_REMATCH_RECEIVED = "REMATCH_RECEIVED"

    def __init__(self, master, my_color, match_id, user_id, client, on_back):
        super().__init__(master, bg="#f0f0f0", width=250)
        self.pack_propagate(False)
        self.my_color = my_color
        self.match_id = match_id
        self.user_id = user_id
        self.client = client
        self.on_back = on_back
        self.game_state = self.STATE_NORMAL

        tk.Label(self, text="Game Controls", font=("Helvetica", 14, "bold"), bg="#f0f0f0").pack(pady=(0, 20))

        self.status_label = tk.Label(self, text="Your turn" if self.my_color == "white" else "Opponent's turn",
                                     font=("Helvetica", 11), bg="#f0f0f0", fg="#2b2b2b", wraplength=220)
        self.status_label.pack(pady=10)

        self.move_label = tk.Label(self, text="Last move: -",
                                   font=("Helvetica", 10), bg="#f0f0f0", wraplength=220)
        self.move_label.pack(pady=10)

        # Action buttons (created once, managed by update_ui_by_state)
        self.btn_pause = tk.Button(self, text="Pause", font=("Helvetica", 11, "bold"), width=18, command=self.pause)
        self.btn_resume = tk.Button(self, text="Resume", font=("Helvetica", 11, "bold"), width=18, command=self.resume)
        self.btn_surrender = tk.Button(self, text="Surrender", font=("Helvetica", 11, "bold"), width=18, command=self.surrender)
        self.btn_offer_draw = tk.Button(self, text="Offer Draw", font=("Helvetica", 11, "bold"), width=18, command=self.offer_draw)
        self.btn_accept_draw = tk.Button(self, text="Accept Draw", font=("Helvetica", 11, "bold"), width=18, command=self.accept_draw, bg="#7fc97f", activebackground="#7fc97f")
        self.btn_decline_draw = tk.Button(self, text="Decline Draw", font=("Helvetica", 11, "bold"), width=18, command=self.decline_draw, bg="#f44336", activebackground="#f44336", fg="white")
        self.btn_rematch = tk.Button(self, text="Rematch", font=("Helvetica", 11, "bold"), width=18, command=self.rematch)
        self.btn_accept_rematch = tk.Button(self, text="Accept Rematch", font=("Helvetica", 11, "bold"), width=18, command=self.accept_rematch, bg="#7fc97f", activebackground="#7fc97f")
        self.btn_decline_rematch = tk.Button(self, text="Decline Rematch", font=("Helvetica", 11, "bold"), width=18, command=self.decline_rematch, bg="#f44336", activebackground="#f44336", fg="white")
        self.btn_back = tk.Button(self, text="Back to Menu", font=("Helvetica", 11, "bold"), width=18, command=self.on_back)

        self.btn_back.pack(pady=(40, 0))
        self.update_ui_by_state()

    def update_ui_by_state(self):
        # Hide all action buttons first
        for btn in [self.btn_pause, self.btn_resume, self.btn_surrender, self.btn_offer_draw,
                    self.btn_accept_draw, self.btn_decline_draw, self.btn_rematch,
                    self.btn_accept_rematch, self.btn_decline_rematch]:
            btn.pack_forget()
        # Show/hide buttons based on state
        if self.game_state == self.STATE_NORMAL:
            self.btn_pause.pack(pady=4)
            self.btn_offer_draw.pack(pady=4)
            self.btn_surrender.pack(pady=4)
        elif self.game_state == self.STATE_PAUSED_BY_ME:
            self.btn_resume.pack(pady=4)
            self.btn_surrender.pack(pady=4)
        elif self.game_state == self.STATE_PAUSED_BY_OPPONENT:
            self.btn_surrender.pack(pady=4)
        elif self.game_state == self.STATE_DRAW_OFFER_SENT:
            self.btn_surrender.pack(pady=4)
        elif self.game_state == self.STATE_DRAW_OFFER_RECEIVED:
            self.btn_accept_draw.pack(pady=4)
            self.btn_decline_draw.pack(pady=4)
            self.btn_surrender.pack(pady=4)
        elif self.game_state == self.STATE_GAME_OVER:
            self.btn_rematch.pack(pady=4)
        elif self.game_state == self.STATE_REMATCH_RECEIVED:
            self.btn_accept_rematch.pack(pady=4)
            self.btn_decline_rematch.pack(pady=4)
        elif self.game_state == self.STATE_REMATCH_SENT:
            pass  # Only show back button

    # Action handlers: send command, update state, call update_ui_by_state
    def pause(self):
        self.client.send(f"PAUSE|{self.match_id}|{self.user_id}\n")
        self.game_state = self.STATE_PAUSED_BY_ME
        self.status_label.config(text="Paused", fg="orange")
        self.update_ui_by_state()

    def resume(self):
        self.client.send(f"RESUME|{self.match_id}|{self.user_id}\n")
        self.game_state = self.STATE_NORMAL
        self.status_label.config(text="Resumed", fg="green")
        self.update_ui_by_state()

    def surrender(self):
        self.client.send(f"SURRENDER|{self.match_id}|{self.user_id}\n")
        self.game_state = self.STATE_GAME_OVER
        self.status_label.config(text="You surrendered", fg="red")
        self.update_ui_by_state()

    def offer_draw(self):
        self.client.send(f"DRAW|{self.match_id}|{self.user_id}\n")
        self.game_state = self.STATE_DRAW_OFFER_SENT
        self.status_label.config(text="Draw offer sent", fg="blue")
        self.update_ui_by_state()

    def accept_draw(self):
        self.client.send(f"DRAW_ACCEPT|{self.match_id}|{self.user_id}\n")
        self.game_state = self.STATE_GAME_OVER
        self.status_label.config(text="Draw accepted", fg="blue")
        self.update_ui_by_state()

    def decline_draw(self):
        self.client.send(f"DRAW_DECLINE|{self.match_id}|{self.user_id}\n")
        self.game_state = self.STATE_NORMAL
        self.status_label.config(text="Draw declined", fg="blue")
        self.update_ui_by_state()

    def rematch(self):
        self.client.send(f"REMATCH|{self.match_id}|{self.user_id}\n")
        self.game_state = self.STATE_REMATCH_SENT
        self.status_label.config(text="Rematch requested", fg="purple")
        self.update_ui_by_state()

    def accept_rematch(self):
        self.client.send(f"REMATCH_ACCEPT|{self.match_id}|{self.user_id}\n")
        self.game_state = self.STATE_NORMAL
        self.status_label.config(text="Rematch accepted", fg="purple")
        self.update_ui_by_state()

    def decline_rematch(self):
        self.client.send(f"REMATCH_DECLINE|{self.match_id}|{self.user_id}\n")
        self.game_state = self.STATE_GAME_OVER
        self.status_label.config(text="Rematch declined", fg="purple")
        self.update_ui_by_state()
