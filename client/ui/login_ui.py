import tkinter as tk
from tkinter import ttk, messagebox
import json
import os

class LoginUI:
    def __init__(self, master, on_login, on_register):
        self.master = master
        self.on_login = on_login
        self.on_register = on_register
        
        # Tạo main frame với màu nền đẹp
        self.frame = tk.Frame(master, bg='#f0f0f0')
        self.frame.pack(expand=True, fill='both')
        
        # Container cho form đăng nhập
        self.login_container = tk.Frame(self.frame, bg='white', relief='flat', bd=0)
        self.login_container.place(relx=0.5, rely=0.5, anchor='center')
        
        # Tạo padding
        inner_frame = tk.Frame(self.login_container, bg='white', padx=40, pady=30)
        inner_frame.pack()
        
        # Icon/Logo
        logo_frame = tk.Frame(inner_frame, bg='white')
        logo_frame.pack(pady=(0, 10))
        
        # Chess icon using Unicode
        chess_label = tk.Label(logo_frame, text="♔ ♕ ♖", font=("Arial", 36), 
                               bg='white', fg='#2c3e50')
        chess_label.pack()
        
        # Title
        title_label = tk.Label(inner_frame, text="ĐĂNG NHẬP", 
                               font=("Arial", 24, "bold"), 
                               bg='white', fg='#2c3e50')
        title_label.pack(pady=(0, 25))
        
        # Username field
        username_frame = tk.Frame(inner_frame, bg='white')
        username_frame.pack(pady=8, fill='x')
        
        tk.Label(username_frame, text="Tên đăng nhập", 
                font=("Arial", 10), bg='white', fg='#555').pack(anchor='w')
        self.username_entry = tk.Entry(username_frame, font=("Arial", 12), 
                                       relief='solid', bd=1, width=30)
        self.username_entry.pack(fill='x', ipady=8)
        
        # Password field
        password_frame = tk.Frame(inner_frame, bg='white')
        password_frame.pack(pady=8, fill='x')
        
        tk.Label(password_frame, text="Mật khẩu", 
                font=("Arial", 10), bg='white', fg='#555').pack(anchor='w')
        self.password_entry = tk.Entry(password_frame, show="•", font=("Arial", 12), 
                                       relief='solid', bd=1, width=30)
        self.password_entry.pack(fill='x', ipady=8)
        
        # Remember me checkbox
        remember_frame = tk.Frame(inner_frame, bg='white')
        remember_frame.pack(pady=10, fill='x')
        
        self.remember_var = tk.BooleanVar()
        remember_cb = tk.Checkbutton(remember_frame, text="Nhớ tài khoản", 
                                     variable=self.remember_var,
                                     font=("Arial", 10), bg='white', 
                                     activebackground='white', 
                                     selectcolor='white')
        remember_cb.pack(anchor='w')

        # Forgot password link
        forgot_link = tk.Label(remember_frame, text="Quên mật khẩu?", 
                               bg='white', fg='#e74c3c', 
                               font=("Arial", 10, "bold", "underline"),
                               cursor='hand2')
        forgot_link.pack(anchor='e', pady=(4, 0))
        forgot_link.bind('<Button-1>', lambda e: self._show_forgot_password())
        
        # Login button
        login_btn = tk.Button(inner_frame, text="ĐĂNG NHẬP", 
                             font=("Arial", 12, "bold"), 
                             bg='#3498db', fg='white', 
                             relief='flat', cursor='hand2',
                             activebackground='#2980b9',
                             activeforeground='white',
                             command=self._login)
        login_btn.pack(pady=(20, 10), fill='x', ipady=10)
        
        # Divider
        divider_frame = tk.Frame(inner_frame, bg='white')
        divider_frame.pack(pady=15, fill='x')
        
        tk.Frame(divider_frame, height=1, bg='#ddd').pack(side='left', fill='x', expand=True)
        tk.Label(divider_frame, text=" hoặc ", bg='white', 
                fg='#888', font=("Arial", 9)).pack(side='left', padx=5)
        tk.Frame(divider_frame, height=1, bg='#ddd').pack(side='left', fill='x', expand=True)
        
        # Register link
        register_frame = tk.Frame(inner_frame, bg='white')
        register_frame.pack()
        
        tk.Label(register_frame, text="Chưa có tài khoản? ", 
                bg='white', fg='#555', font=("Arial", 10)).pack(side='left')
        
        register_link = tk.Label(register_frame, text="Đăng ký ngay", 
                                bg='white', fg='#3498db', 
                                font=("Arial", 10, "bold", "underline"),
                                cursor='hand2')
        register_link.pack(side='left')
        register_link.bind('<Button-1>', lambda e: self._show_register())
        
        # Load saved credentials
        self._load_credentials()
        
        # Bind Enter key
        self.username_entry.bind('<Return>', lambda e: self.password_entry.focus())
        self.password_entry.bind('<Return>', lambda e: self._login())

    def _load_credentials(self):
        """Tải thông tin đăng nhập đã lưu"""
        try:
            cred_file = os.path.join(os.path.dirname(__file__), '..', '.credentials')
            if os.path.exists(cred_file):
                with open(cred_file, 'r') as f:
                    creds = json.load(f)
                    self.username_entry.insert(0, creds.get('username', ''))
                    self.password_entry.insert(0, creds.get('password', ''))
                    self.remember_var.set(True)
        except Exception as e:
            pass

    def _save_credentials(self):
        """Lưu thông tin đăng nhập"""
        try:
            cred_file = os.path.join(os.path.dirname(__file__), '..', '.credentials')
            if self.remember_var.get():
                with open(cred_file, 'w') as f:
                    json.dump({
                        'username': self.username_entry.get(),
                        'password': self.password_entry.get()
                    }, f)
            else:
                # Xóa file nếu không chọn remember
                if os.path.exists(cred_file):
                    os.remove(cred_file)
        except Exception as e:
            pass

    def _login(self):
        username = self.username_entry.get().strip()
        password = self.password_entry.get().strip()
        
        if not username or not password:
            messagebox.showwarning("Cảnh báo", "Vui lòng nhập đầy đủ thông tin!")
            return
        
        self._save_credentials()
        self.on_login(username, password)

    def _show_register(self):
        """Chuyển sang giao diện đăng ký"""
        # Gọi callback để game_ui.py xử lý việc chuyển UI
        if hasattr(self, 'on_switch_to_register'):
            self.on_switch_to_register()

    def _show_forgot_password(self):
        """Chuyển sang giao diện quên mật khẩu"""
        if hasattr(self, 'on_switch_to_forgot'):
            self.on_switch_to_forgot()
    
    def set_switch_callback(self, callback):
        """Set callback để chuyển sang register"""
        self.on_switch_to_register = callback

    def set_forgot_callback(self, callback):
        """Set callback để chuyển sang quên mật khẩu"""
        self.on_switch_to_forgot = callback

    def destroy(self):
        self.frame.destroy()


class ForgotPasswordUI:
    def __init__(self, master, on_request_otp, on_reset_password, on_back_to_login):
        self.master = master
        self.on_request_otp = on_request_otp
        self.on_reset_password = on_reset_password
        self.on_back_to_login = on_back_to_login
        
        self.user_id = None
        self.email = None
        self.step = 1  # 1: nhập username, 2: nhập OTP + password
        
        # Tạo main frame với màu nền đẹp
        self.frame = tk.Frame(master, bg='#f0f0f0')
        self.frame.pack(expand=True, fill='both')
        
        # Container cho form
        self.container = tk.Frame(self.frame, bg='white', relief='flat', bd=0)
        self.container.place(relx=0.5, rely=0.5, anchor='center')
        
        self._show_step1()
    
    def _show_step1(self):
        """Bước 1: Nhập email để nhận OTP"""
        # Clear container
        for widget in self.container.winfo_children():
            widget.destroy()
        
        # Tạo padding
        inner_frame = tk.Frame(self.container, bg='white', padx=40, pady=30)
        inner_frame.pack()
        
        # Icon/Logo
        logo_frame = tk.Frame(inner_frame, bg='white')
        logo_frame.pack(pady=(0, 10))
        
        # Lock icon
        lock_label = tk.Label(logo_frame, text="🔒", font=("Arial", 36), 
                              bg='white', fg='#e74c3c')
        lock_label.pack()
        
        # Title
        title_label = tk.Label(inner_frame, text="QUÊN MẬT KHẨU", 
                               font=("Arial", 24, "bold"), 
                               bg='white', fg='#e74c3c')
        title_label.pack(pady=(0, 10))
        
        # Subtitle
        subtitle_label = tk.Label(inner_frame, 
                      text="Nhập email để nhận mã OTP", 
                      font=("Arial", 10), 
                      bg='white', fg='#666')
        subtitle_label.pack(pady=(0, 20))
        
        # Email field
        email_frame = tk.Frame(inner_frame, bg='white')
        email_frame.pack(pady=8, fill='x')

        tk.Label(email_frame, text="Email", 
            font=("Arial", 10), bg='white', fg='#555').pack(anchor='w')
        self.email_entry = tk.Entry(email_frame, font=("Arial", 12), 
                         relief='solid', bd=1, width=30)
        self.email_entry.pack(fill='x', ipady=8)
        
        # Send OTP button
        send_btn = tk.Button(inner_frame, text="GỬI MÃ OTP", 
                            font=("Arial", 12, "bold"), 
                            bg='#e74c3c', fg='white', 
                            relief='flat', cursor='hand2',
                            activebackground='#c0392b',
                            activeforeground='white',
                            command=self._request_otp)
        send_btn.pack(pady=(20, 10), fill='x', ipady=10)
        
        # Back to login link
        back_frame = tk.Frame(inner_frame, bg='white')
        back_frame.pack(pady=15)
        
        back_link = tk.Label(back_frame, text="← Quay lại đăng nhập", 
                            bg='white', fg='#3498db', 
                            font=("Arial", 10, "bold"),
                            cursor='hand2')
        back_link.pack()
        back_link.bind('<Button-1>', lambda e: self._back_to_login())
        
        # Bind Enter key
        self.email_entry.bind('<Return>', lambda e: self._request_otp())
        self.email_entry.focus()
    
    def _request_otp(self):
        """Yêu cầu gửi OTP"""
        email = self.email_entry.get().strip()
        
        if not email:
            messagebox.showwarning("Cảnh báo", "Vui lòng nhập email!")
            return
        
        # Call callback to request OTP
        self.on_request_otp(email)
    
    def show_step2(self, user_id, email):
        """Bước 2: Nhập OTP và mật khẩu mới"""
        self.user_id = user_id
        self.email = email
        self.step = 2
        
        # Clear container
        for widget in self.container.winfo_children():
            widget.destroy()
        
        # Tạo padding
        inner_frame = tk.Frame(self.container, bg='white', padx=40, pady=30)
        inner_frame.pack()
        
        # Icon/Logo
        logo_frame = tk.Frame(inner_frame, bg='white')
        logo_frame.pack(pady=(0, 10))
        
        # Check icon
        check_label = tk.Label(logo_frame, text="✉️", font=("Arial", 36), 
                               bg='white')
        check_label.pack()
        
        # Title
        title_label = tk.Label(inner_frame, text="NHẬP MÃ OTP", 
                               font=("Arial", 24, "bold"), 
                               bg='white', fg='#e74c3c')
        title_label.pack(pady=(0, 10))
        
        # Subtitle
        masked_email = self._mask_email(email)
        subtitle_label = tk.Label(inner_frame, 
                                  text=f"Mã OTP đã được gửi đến {masked_email}", 
                                  font=("Arial", 10), 
                                  bg='white', fg='#666')
        subtitle_label.pack(pady=(0, 20))
        
        # OTP field
        otp_frame = tk.Frame(inner_frame, bg='white')
        otp_frame.pack(pady=8, fill='x')
        
        tk.Label(otp_frame, text="Mã OTP (6 chữ số)", 
                font=("Arial", 10), bg='white', fg='#555').pack(anchor='w')
        self.otp_entry = tk.Entry(otp_frame, font=("Arial", 14, "bold"), 
                                  relief='solid', bd=1, width=30,
                                  justify='center')
        self.otp_entry.pack(fill='x', ipady=8)
        
        # New Password field
        password_frame = tk.Frame(inner_frame, bg='white')
        password_frame.pack(pady=8, fill='x')
        
        tk.Label(password_frame, text="Mật khẩu mới", 
                font=("Arial", 10), bg='white', fg='#555').pack(anchor='w')
        self.password_entry = tk.Entry(password_frame, show="•", font=("Arial", 12), 
                                       relief='solid', bd=1, width=30)
        self.password_entry.pack(fill='x', ipady=8)
        
        # Confirm Password field
        confirm_password_frame = tk.Frame(inner_frame, bg='white')
        confirm_password_frame.pack(pady=8, fill='x')
        
        tk.Label(confirm_password_frame, text="Xác nhận mật khẩu mới", 
                font=("Arial", 10), bg='white', fg='#555').pack(anchor='w')
        self.confirm_password_entry = tk.Entry(confirm_password_frame, show="•", 
                                               font=("Arial", 12), 
                                               relief='solid', bd=1, width=30)
        self.confirm_password_entry.pack(fill='x', ipady=8)
        
        # Reset button
        reset_btn = tk.Button(inner_frame, text="ĐẶT LẠI MẬT KHẨU", 
                             font=("Arial", 12, "bold"), 
                             bg='#27ae60', fg='white', 
                             relief='flat', cursor='hand2',
                             activebackground='#229954',
                             activeforeground='white',
                             command=self._reset_password)
        reset_btn.pack(pady=(20, 10), fill='x', ipady=10)
        
        # Back to login link
        back_frame = tk.Frame(inner_frame, bg='white')
        back_frame.pack(pady=15)
        
        back_link = tk.Label(back_frame, text="← Quay lại đăng nhập", 
                            bg='white', fg='#3498db', 
                            font=("Arial", 10, "bold"),
                            cursor='hand2')
        back_link.pack()
        back_link.bind('<Button-1>', lambda e: self._back_to_login())
        
        # Bind Enter keys
        self.otp_entry.bind('<Return>', lambda e: self.password_entry.focus())
        self.password_entry.bind('<Return>', lambda e: self.confirm_password_entry.focus())
        self.confirm_password_entry.bind('<Return>', lambda e: self._reset_password())
        self.otp_entry.focus()
    
    def _mask_email(self, email):
        """Che email để bảo mật: test@example.com -> t***@example.com"""
        if '@' not in email:
            return email
        parts = email.split('@')
        if len(parts[0]) <= 2:
            return email
        return parts[0][0] + '***' + '@' + parts[1]
    
    def _reset_password(self):
        """Đặt lại mật khẩu"""
        otp = self.otp_entry.get().strip()
        password = self.password_entry.get().strip()
        confirm_password = self.confirm_password_entry.get().strip()
        
        # Validate
        if not otp or not password or not confirm_password:
            messagebox.showwarning("Cảnh báo", "Vui lòng nhập đầy đủ thông tin!")
            return
        
        if len(otp) != 6 or not otp.isdigit():
            messagebox.showerror("Lỗi", "Mã OTP phải là 6 chữ số!")
            self.otp_entry.delete(0, tk.END)
            self.otp_entry.focus()
            return
        
        if password != confirm_password:
            messagebox.showerror("Lỗi", "Mật khẩu xác nhận không khớp!")
            self.confirm_password_entry.delete(0, tk.END)
            self.confirm_password_entry.focus()
            return
        
        if len(password) < 6:
            messagebox.showwarning("Cảnh báo", "Mật khẩu phải có ít nhất 6 ký tự!")
            return
        
        # Call callback to reset password
        self.on_reset_password(self.user_id, otp, password)
    
    def _back_to_login(self):
        """Quay lại đăng nhập"""
        if hasattr(self, 'on_back_to_login') and self.on_back_to_login:
            self.on_back_to_login()
    
    def show_success_and_redirect(self):
        """Hiển thị thành công và chuyển về đăng nhập"""
        messagebox.showinfo("Thành công", "Đặt lại mật khẩu thành công! Vui lòng đăng nhập.")
        self._back_to_login()
    
    def destroy(self):
        self.frame.destroy()


class RegisterUI:
    def __init__(self, master, on_register, on_back_to_login):
        self.master = master
        self.on_register = on_register
        self.on_back_to_login = on_back_to_login
        
        # Tạo main frame với màu nền đẹp
        self.frame = tk.Frame(master, bg='#f0f0f0')
        self.frame.pack(expand=True, fill='both')
        
        # Container cho form đăng ký
        self.register_container = tk.Frame(self.frame, bg='white', relief='flat', bd=0)
        self.register_container.place(relx=0.5, rely=0.5, anchor='center')
        
        # Tạo padding
        inner_frame = tk.Frame(self.register_container, bg='white', padx=40, pady=30)
        inner_frame.pack()
        
        # Icon/Logo
        logo_frame = tk.Frame(inner_frame, bg='white')
        logo_frame.pack(pady=(0, 10))
        
        # Chess icon using Unicode
        chess_label = tk.Label(logo_frame, text="♔ ♕ ♖", font=("Arial", 36), 
                               bg='white', fg='#27ae60')
        chess_label.pack()
        
        # Title
        title_label = tk.Label(inner_frame, text="ĐĂNG KÝ TÀI KHOẢN", 
                               font=("Arial", 24, "bold"), 
                               bg='white', fg='#27ae60')
        title_label.pack(pady=(0, 25))
        
        # Username field
        username_frame = tk.Frame(inner_frame, bg='white')
        username_frame.pack(pady=8, fill='x')
        
        tk.Label(username_frame, text="Tên đăng nhập", 
                font=("Arial", 10), bg='white', fg='#555').pack(anchor='w')
        self.username_entry = tk.Entry(username_frame, font=("Arial", 12), 
                                       relief='solid', bd=1, width=30)
        self.username_entry.pack(fill='x', ipady=8)
        
        # Email field
        email_frame = tk.Frame(inner_frame, bg='white')
        email_frame.pack(pady=8, fill='x')
        
        tk.Label(email_frame, text="Email", 
                font=("Arial", 10), bg='white', fg='#555').pack(anchor='w')
        self.email_entry = tk.Entry(email_frame, font=("Arial", 12), 
                                    relief='solid', bd=1, width=30)
        self.email_entry.pack(fill='x', ipady=8)
        
        # Password field
        password_frame = tk.Frame(inner_frame, bg='white')
        password_frame.pack(pady=8, fill='x')
        
        tk.Label(password_frame, text="Mật khẩu", 
                font=("Arial", 10), bg='white', fg='#555').pack(anchor='w')
        self.password_entry = tk.Entry(password_frame, show="•", font=("Arial", 12), 
                                       relief='solid', bd=1, width=30)
        self.password_entry.pack(fill='x', ipady=8)
        
        # Confirm Password field
        confirm_password_frame = tk.Frame(inner_frame, bg='white')
        confirm_password_frame.pack(pady=8, fill='x')
        
        tk.Label(confirm_password_frame, text="Xác nhận mật khẩu", 
                font=("Arial", 10), bg='white', fg='#555').pack(anchor='w')
        self.confirm_password_entry = tk.Entry(confirm_password_frame, show="•", 
                                               font=("Arial", 12), 
                                               relief='solid', bd=1, width=30)
        self.confirm_password_entry.pack(fill='x', ipady=8)
        
        # Register button
        register_btn = tk.Button(inner_frame, text="ĐĂNG KÝ", 
                                font=("Arial", 12, "bold"), 
                                bg='#27ae60', fg='white', 
                                relief='flat', cursor='hand2',
                                activebackground='#229954',
                                activeforeground='white',
                                command=self._register)
        register_btn.pack(pady=(20, 10), fill='x', ipady=10)
        
        # Divider
        divider_frame = tk.Frame(inner_frame, bg='white')
        divider_frame.pack(pady=15, fill='x')
        
        tk.Frame(divider_frame, height=1, bg='#ddd').pack(side='left', fill='x', expand=True)
        tk.Label(divider_frame, text=" hoặc ", bg='white', 
                fg='#888', font=("Arial", 9)).pack(side='left', padx=5)
        tk.Frame(divider_frame, height=1, bg='#ddd').pack(side='left', fill='x', expand=True)
        
        # Login link
        login_frame = tk.Frame(inner_frame, bg='white')
        login_frame.pack()
        
        tk.Label(login_frame, text="Đã có tài khoản? ", 
                bg='white', fg='#555', font=("Arial", 10)).pack(side='left')
        
        login_link = tk.Label(login_frame, text="Đăng nhập ngay", 
                             bg='white', fg='#3498db', 
                             font=("Arial", 10, "bold", "underline"),
                             cursor='hand2')
        login_link.pack(side='left')
        login_link.bind('<Button-1>', lambda e: self._back_to_login())
        
        # Bind Enter key
        self.username_entry.bind('<Return>', lambda e: self.email_entry.focus())
        self.email_entry.bind('<Return>', lambda e: self.password_entry.focus())
        self.password_entry.bind('<Return>', lambda e: self.confirm_password_entry.focus())
        self.confirm_password_entry.bind('<Return>', lambda e: self._register())

    def _register(self):
        username = self.username_entry.get().strip()
        email = self.email_entry.get().strip()
        password = self.password_entry.get().strip()
        confirm_password = self.confirm_password_entry.get().strip()
        
        # Validate inputs
        if not username or not email or not password or not confirm_password:
            messagebox.showwarning("Cảnh báo", "Vui lòng nhập đầy đủ thông tin!")
            return
        
        if password != confirm_password:
            messagebox.showerror("Lỗi", "Mật khẩu xác nhận không khớp!")
            self.confirm_password_entry.delete(0, tk.END)
            self.confirm_password_entry.focus()
            return
        
        if len(password) < 6:
            messagebox.showwarning("Cảnh báo", "Mật khẩu phải có ít nhất 6 ký tự!")
            return
        
        # Call register callback
        self.on_register(username, password, email)
    
    def _back_to_login(self):
        """Quay lại trang đăng nhập"""
        # Gọi callback để game_ui.py xử lý việc chuyển UI
        if hasattr(self, 'on_back_to_login') and self.on_back_to_login:
            self.on_back_to_login()
    
    def show_success_and_redirect(self):
        """Hiển thị thông báo thành công và chuyển về đăng nhập"""
        messagebox.showinfo("Thành công", "Đăng ký thành công! Vui lòng đăng nhập.")
        self._back_to_login()
    
    def destroy(self):
        self.frame.destroy()
