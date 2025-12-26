import tkinter as tk
from ui.game_ui import ChessApp

def main():
    root = tk.Tk()
    root.title("Chess Client - LTM")
    app = ChessApp(root)
    root.mainloop()

if __name__ == "__main__":
    main()

