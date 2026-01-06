import tkinter as tk


def show_toast(master, message: str, kind: str = "info", duration: int = 3000):
    """Show a small toast notification at the top-right of the master window.

    kind: "success" (green), "error" (red), "info" (blue), "warning" (orange)
    duration: milliseconds before auto-hide
    """
    try:
        toast = tk.Toplevel(master)
        toast.overrideredirect(True)
        toast.attributes("-topmost", True)

        colors = {
            "success": ("#e6ffed", "#22863a"),
            "error": ("#ffeef0", "#b31d28"),
            "info": ("#e7f3ff", "#0366d6"),
            "warning": ("#fff5e6", "#b08800"),
        }
        bg, fg = colors.get(kind, colors["info"])

        frame = tk.Frame(toast, bg=bg, bd=1, relief="solid")
        frame.pack(fill="both", expand=True)
        label = tk.Label(frame, text=message, bg=bg, fg=fg, font=("Helvetica", 10))
        label.pack(padx=12, pady=8)

        toast.update_idletasks()
        width = toast.winfo_width()
        height = toast.winfo_height()

        # Position relative to master window (top-right)
        try:
            mx = master.winfo_x()
            my = master.winfo_y()
            mw = master.winfo_width()
        except tk.TclError:
            # Fallback to screen coordinates
            mx, my, mw = 0, 0, master.winfo_screenwidth()

        margin = 16
        x = mx + mw - width - margin
        y = my + margin
        toast.geometry(f"{width}x{height}+{x}+{y}")

        # Auto-destroy after duration
        toast.after(duration, toast.destroy)
    except Exception:
        # Fallback to messagebox if toast fails
        from tkinter import messagebox
        if kind == "error":
            messagebox.showerror("Thông báo", message)
        elif kind == "warning":
            messagebox.showwarning("Thông báo", message)
        else:
            messagebox.showinfo("Thông báo", message)


def show_result_overlay(master, result_text: str, new_elo: int | None, elo_delta: int | None):
    """Show a centered overlay with match result and ELO change."""
    overlay = tk.Toplevel(master)
    overlay.title("Kết quả trận đấu")
    overlay.transient(master)
    overlay.attributes("-topmost", True)
    overlay.resizable(False, False)

    bg = "#ffffff"
    frame = tk.Frame(overlay, bg=bg, bd=1, relief="solid")
    frame.pack(fill="both", expand=True, padx=12, pady=12)

    title_color = "#22863a" if "won" in result_text.lower() else ("#b31d28" if "lost" in result_text.lower() else "#0366d6")
    tk.Label(frame, text=result_text, font=("Helvetica", 16, "bold"), bg=bg, fg=title_color).pack(pady=(8, 12))

    if new_elo is not None and elo_delta is not None:
        delta_text = f"+{elo_delta}" if elo_delta > 0 else (f"{elo_delta}" if elo_delta < 0 else "+0")
        delta_color = "#22863a" if elo_delta > 0 else ("#b31d28" if elo_delta < 0 else "#6a737d")
        tk.Label(frame, text=f"ELO mới: {new_elo}", font=("Helvetica", 12), bg=bg, fg="#1f2937").pack(pady=(0, 4))
        tk.Label(frame, text=f"Thay đổi: {delta_text}", font=("Helvetica", 12, "bold"), bg=bg, fg=delta_color).pack(pady=(0, 8))
    else:
        tk.Label(frame, text="ELO: (đang cập nhật)", font=("Helvetica", 12), bg=bg, fg="#6a737d").pack(pady=(0, 8))

    btn = tk.Button(frame, text="Đóng", command=overlay.destroy)
    btn.pack(pady=(6, 8))

    overlay.update_idletasks()
    w = overlay.winfo_width()
    h = overlay.winfo_height()
    try:
        mx = master.winfo_x()
        my = master.winfo_y()
        mw = master.winfo_width()
        mh = master.winfo_height()
    except tk.TclError:
        # Fallback center on screen
        mx, my, mw, mh = 0, 0, master.winfo_screenwidth(), master.winfo_screenheight()

    x = mx + (mw - w) // 2
    y = my + (mh - h) // 2
    overlay.geometry(f"{w}x{h}+{x}+{y}")
