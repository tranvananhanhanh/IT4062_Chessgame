import tkinter as tk

class LoginUI:
    def __init__(self, master, on_login, on_register):
        self.frame = tk.Frame(master, padx=20, pady=20)
        self.frame.pack(expand=True)
        self.on_login = on_login
        self.on_register = on_register

        tk.Label(self.frame, text="Chess Client", font=("Helvetica", 18, "bold")).grid(row=0, column=0, columnspan=2, pady=(0, 20))

        tk.Label(self.frame, text="Username:", font=("Helvetica", 11)).grid(row=1, column=0, sticky="e", pady=5)
        self.username_entry = tk.Entry(self.frame, width=25)
        self.username_entry.grid(row=1, column=1)

        tk.Label(self.frame, text="Password:", font=("Helvetica", 11)).grid(row=2, column=0, sticky="e", pady=5)
        self.password_entry = tk.Entry(self.frame, show="*", width=25)
        self.password_entry.grid(row=2, column=1)

        tk.Label(self.frame, text="Email:", font=("Helvetica", 11)).grid(row=3, column=0, sticky="e", pady=5)
        self.email_entry = tk.Entry(self.frame, width=25)
        self.email_entry.grid(row=3, column=1)

        login_btn = tk.Button(self.frame, text="Login", font=("Helvetica", 11, "bold"), width=20, pady=5, command=self._login)
        login_btn.grid(row=4, column=0, columnspan=2, pady=(15, 5))
        register_btn = tk.Button(self.frame, text="Register", font=("Helvetica", 11, "bold"), width=20, pady=5, command=self._register)
        register_btn.grid(row=5, column=0, columnspan=2)

    def _login(self):
        username = self.username_entry.get()
        password = self.password_entry.get()
        self.on_login(username, password)

    def _register(self):
        username = self.username_entry.get()
        password = self.password_entry.get()
        email = self.email_entry.get()
        self.on_register(username, password, email)

    def destroy(self):
        self.frame.destroy()
