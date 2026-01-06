import tkinter as tk
from tkinter import scrolledtext
from datetime import datetime

class GameChatUI(tk.Frame):
    """In-game chat interface for PvP matches"""
    
    def __init__(self, master, match_id, client, send_callback=None):
        super().__init__(master, bg="#e8e8e8", width=250)
        self.pack_propagate(False)
        
        self.match_id = match_id
        self.client = client
        self.send_callback = send_callback  # Callback when message is sent
        
        # Header
        tk.Label(
            self,
            text="Match Chat",
            font=("Helvetica", 12, "bold"),
            bg="#e8e8e8"
        ).pack(pady=5, padx=5, fill="x")
        
        # Chat display area (read-only)
        self.chat_display = scrolledtext.ScrolledText(
            self,
            width=30,
            height=15,
            state="disabled",
            bg="#ffffff",
            fg="#2b2b2b",
            font=("Helvetica", 9),
            wrap=tk.WORD
        )
        self.chat_display.pack(padx=5, pady=(5, 10), fill="both", expand=True)
        
        # Input area frame
        input_frame = tk.Frame(self, bg="#e8e8e8")
        input_frame.pack(padx=5, pady=(0, 5), fill="x")
        
        # Message input field
        self.message_input = tk.Entry(
            input_frame,
            font=("Helvetica", 9),
            bg="#ffffff",
            fg="#2b2b2b"
        )
        self.message_input.pack(side="left", fill="both", expand=True, padx=(0, 5))
        self.message_input.bind("<Return>", lambda e: self.send_message())
        
        # Send button
        self.btn_send = tk.Button(
            input_frame,
            text="Send",
            width=8,
            command=self.send_message,
            bg="#7fc97f",
            fg="white",
            font=("Helvetica", 9, "bold")
        )
        self.btn_send.pack(side="right")
        
        # Configure text tag styles
        self.chat_display.tag_config("opponent", foreground="#d32f2f", font=("Helvetica", 9, "bold"))
        self.chat_display.tag_config("self", foreground="#1976d2", font=("Helvetica", 9, "bold"))
        self.chat_display.tag_config("timestamp", foreground="#999999", font=("Helvetica", 8))
        self.chat_display.tag_config("system", foreground="#666666", font=("Helvetica", 9, "italic"))
    
    def add_message(self, sender_name, message, is_self=False):
        """Add a message to chat display"""
        self.chat_display.config(state="normal")
        
        if is_self:
            self.chat_display.insert("end", f"You: ", "self")
        else:
            self.chat_display.insert("end", f"{sender_name}: ", "opponent")
        
        self.chat_display.insert("end", f"{message}\n")
        
        # Auto-scroll to bottom
        self.chat_display.see("end")
        self.chat_display.config(state="disabled")
    
    def add_system_message(self, message):
        """Add a system message (no sender)"""
        self.chat_display.config(state="normal")
        
        self.chat_display.insert("end", f"{message}\n", "system")
        
        self.chat_display.see("end")
        self.chat_display.config(state="disabled")
    
    def send_message(self):
        """Send chat message"""
        message = self.message_input.get().strip()
        
        if not message:
            return
        
        # Clear input
        self.message_input.delete(0, tk.END)
        
        # Add to display immediately (optimistic update)
        self.add_message("You", message, is_self=True)
        
        # Send to server
        if self.client:
            try:
                cmd = f"GAME_CHAT|{self.match_id}|{message}"
                self.client.send(cmd)
            except Exception as e:
                print(f"[GameChat] Error sending message: {e}")
                self.add_system_message(f"Failed to send message: {str(e)}")
        
        # Call callback if provided
        if self.send_callback:
            self.send_callback(message)
    
    def clear_chat(self):
        """Clear all messages"""
        self.chat_display.config(state="normal")
        self.chat_display.delete("1.0", "end")
        self.chat_display.config(state="disabled")
