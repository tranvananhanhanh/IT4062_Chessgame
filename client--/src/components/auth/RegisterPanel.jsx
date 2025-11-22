import { useState } from 'react'

const API_BASE_URL = import.meta.env.VITE_API_BASE_URL || 'http://localhost:5001'

const RegisterPanel = ({ onClose, onRegisterSuccess }) => {
  const [form, setForm] = useState({
    username: '',
    email: '',
    password: '',
    confirmPassword: '',
    agree: false,
  })
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')
  const [success, setSuccess] = useState('')

  const handleChange = (event) => {
    const { name, value, type, checked } = event.target
    setForm((prev) => ({
      ...prev,
      [name]: type === 'checkbox' ? checked : value,
    }))
  }

  const handleSubmit = async (event) => {
    event.preventDefault()
    setError('')
    setSuccess('')

    if (!form.agree) {
      setError('Bạn cần đồng ý với Điều khoản & Chính sách bảo mật.')
      return
    }

    if (form.password.length < 6) {
      setError('Mật khẩu phải có ít nhất 6 ký tự.')
      return
    }

    if (form.password !== form.confirmPassword) {
      setError('Mật khẩu xác nhận không khớp.')
      return
    }

    setLoading(true)
    try {
      const response = await fetch(`${API_BASE_URL}/api/register`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          username: form.username,
          email: form.email,
          password: form.password,
        }),
      })

      const data = await response.json().catch(() => ({}))

      if (!response.ok || data.success === false) {
        setError(data.message || 'Đăng ký thất bại, vui lòng thử lại.')
        return
      }

      setSuccess(data.message || 'Đăng ký thành công.')
      setError('')

      // Báo cho App biết nếu cần
      if (typeof onRegisterSuccess === 'function') {
        onRegisterSuccess(data.user)
      }

      // Thông báo + đóng panel
      alert(data.message || 'Đăng ký thành công!')
      if (typeof onClose === 'function') {
        onClose()
      }
    } catch (err) {
      console.error('Register error:', err)
      setError('Không thể kết nối tới server. Vui lòng kiểm tra lại backend.')
    } finally {
      setLoading(false)
    }
  }

  return (
    <div className="login-panel">
      <button
        type="button"
        className="close-login"
        aria-label="Đóng đăng ký"
        onClick={onClose}
      >
        ×
      </button>

      <form className="login-form" onSubmit={handleSubmit}>
        {/* Username */}
        <label className="input-wrapper">
          <span className="input-icon" aria-hidden="true">
            👤
          </span>
          <input
            type="text"
            name="username"
            placeholder="Username"
            required
            value={form.username}
            onChange={handleChange}
          />
        </label>

        {/* Email */}
        <label className="input-wrapper">
          <span className="input-icon" aria-hidden="true">
            📧
          </span>
          <input
            type="email"
            name="email"
            placeholder="Email"
            required
            value={form.email}
            onChange={handleChange}
          />
        </label>

        {/* Password */}
        <label className="input-wrapper">
          <span className="input-icon" aria-hidden="true">
            🔒
          </span>
          <input
            type="password"
            name="password"
            placeholder="Password"
            minLength={6}
            required
            value={form.password}
            onChange={handleChange}
          />
        </label>

        {/* Confirm password */}
        <label className="input-wrapper">
          <span className="input-icon" aria-hidden="true">
            🔒
          </span>
          <input
            type="password"
            name="confirmPassword"
            placeholder="Confirm password"
            minLength={6}
            required
            value={form.confirmPassword}
            onChange={handleChange}
          />
        </label>

        {/* Điều khoản */}
        <div className="remember-row">
          <label className="remember-checkbox">
            <input
              type="checkbox"
              name="agree"
              checked={form.agree}
              onChange={handleChange}
              required
            />
            <span>Tôi đồng ý với Điều khoản &amp; Chính sách bảo mật</span>
          </label>
        </div>

        {/* Hiển thị lỗi / thành công */}
        {error && <p className="form-error" style={{ color: 'red', marginBottom: '8px' }}>{error}</p>}
        {success && <p className="form-success" style={{ color: 'green', marginBottom: '8px' }}>{success}</p>}

        {/* Nút đăng ký */}
        <button type="submit" className="login-btn" disabled={loading}>
          {loading ? 'Đang tạo tài khoản...' : 'Create account'}
        </button>

        {/* Divider */}
        <div className="divider">
          <span />
          or
          <span />
        </div>

        {/* Social sign up: Google / Facebook / Apple */}
        <div className="social-login">
          {[
            { id: 'google', label: 'Sign up with Google', icon: 'G' },
            { id: 'facebook', label: 'Sign up with Facebook', icon: 'f' },
            { id: 'apple', label: 'Sign up with Apple', icon: '' },
          ].map((provider) => (
            <button
              key={provider.id}
              type="button"
              className={`social-btn social-${provider.id}`}
              onClick={() =>
                alert(`${provider.label} hiện mới là UI demo, chưa kết nối OAuth.`)
              }
            >
              <span className="social-icon">{provider.icon}</span>
              {provider.label}
            </button>
          ))}
        </div>
      </form>
    </div>
  )
}

export default RegisterPanel
