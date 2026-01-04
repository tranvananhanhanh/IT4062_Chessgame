import tkinter as tk
from utils.credentials_cache import CredentialsCache

class LoginUI:
    def __init__(self, master, on_login, on_register_click):
        self.frame = tk.Frame(master, padx=20, pady=20)
        self.frame.pack(expand=True)
        self.on_login = on_login
        self.on_register_click = on_register_click
        self.cache = CredentialsCache()

        tk.Label(self.frame, text="Chess Client", font=("Helvetica", 18, "bold")).grid(row=0, column=0, columnspan=2, pady=(0, 20))

        tk.Label(self.frame, text="Username:", font=("Helvetica", 11)).grid(row=1, column=0, sticky="e", pady=5)
        self.username_entry = tk.Entry(self.frame, width=25)
        self.username_entry.grid(row=1, column=1)

        tk.Label(self.frame, text="Password:", font=("Helvetica", 11)).grid(row=2, column=0, sticky="e", pady=5)
        self.password_entry = tk.Entry(self.frame, show="*", width=25)
        self.password_entry.grid(row=2, column=1)

        # Checkbox để nhớ tài khoản
        self.remember_var = tk.BooleanVar(value=False)
        tk.Checkbutton(self.frame, text="Nhớ tài khoản", variable=self.remember_var, font=("Helvetica", 10)).grid(row=3, column=1, sticky="w", pady=5)

        login_btn = tk.Button(self.frame, text="Login", font=("Helvetica", 11, "bold"), width=20, pady=5, command=self._login)
        login_btn.grid(row=4, column=0, columnspan=2, pady=(15, 5))
        
        register_btn = tk.Button(self.frame, text="Đăng ký", font=("Helvetica", 11, "bold"), width=20, pady=5, command=self._register)
        register_btn.grid(row=5, column=0, columnspan=2)

        # Load cached credentials nếu có
        self.load_cached_credentials()

    def _login(self):
        username = self.username_entry.get()
        password = self.password_entry.get()
        
        # Lưu cache nếu checkbox được tích
        if self.remember_var.get():
            self.cache.save_credentials(username, password)
        else:
            self.cache.clear_credentials()
        
        self.on_login(username, password)

    def _register(self):
        self.on_register_click()

    def load_cached_credentials(self):
        """Tải credentials từ cache nếu có"""
        if self.cache.has_cached_credentials():
            creds = self.cache.load_credentials()
            if creds["username"]:
                self.username_entry.insert(0, creds["username"])
                self.password_entry.insert(0, creds["password"])
                self.remember_var.set(True)

    def destroy(self):
        self.frame.destroy()
