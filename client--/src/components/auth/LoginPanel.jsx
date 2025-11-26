const LoginPanel = ({ onSubmit, onForgot, onClose }) => (
  <div className="login-panel">
    <button type="button" className="close-login" aria-label="Đóng đăng nhập" onClick={onClose}>
      ×
    </button>
    <form className="login-form" onSubmit={onSubmit}>
      <label className="input-wrapper">
        <span className="input-icon" aria-hidden="true">
          👤
        </span>
        <input type="text" placeholder="Username, Phone, or Email" required />
      </label>
      <label className="input-wrapper">
        <span className="input-icon" aria-hidden="true">
          🔒
        </span>
        <input type="password" placeholder="Password" required />
        <button type="button" className="input-addon" aria-label="Hiện mật khẩu">
          👁
        </button>
      </label>
      <div className="remember-row">
        <label className="remember-checkbox">
          <input type="checkbox" defaultChecked />
          <span>Remember me</span>
        </label>
        <button type="button" onClick={onForgot}>
          Forgot password?
        </button>
      </div>
      <button type="submit" className="login-btn">
        Log In
      </button>
      <div className="divider">
        <span />
        <p>OR</p>
        <span />
      </div>
      <div className="social-login">
        {[
          { id: 'apple', label: 'Log in with Apple', icon: '' },
          { id: 'google', label: 'Log in with Google', icon: 'G' },
          { id: 'facebook', label: 'Log in with Facebook', icon: 'f' },
        ].map((provider) => (
          <button key={provider.id} type="button" className={`social-btn social-${provider.id}`}>
            <span className="social-icon">{provider.icon}</span>
            {provider.label}
          </button>
        ))}
      </div>
    </form>
  </div>
)

export default LoginPanel
