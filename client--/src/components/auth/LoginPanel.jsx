import { useState } from 'react'

const LoginPanel = ({ onSubmit, onForgot, onClose, isLoading = false, error = '' }) => {
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [showPassword, setShowPassword] = useState(false)

  const handleSubmit = (event) => {
    event.preventDefault()
    onSubmit?.({ username, password })
  }

  return (
    <div className="login-panel">
      <button type="button" className="close-login" aria-label="Close login" onClick={onClose}>
        X
      </button>
      <form className="login-form" onSubmit={handleSubmit}>
        <label className="input-wrapper">
          <span className="input-icon" aria-hidden="true">
            @
          </span>
          <input
            type="text"
            name="username"
            value={username}
            onChange={(e) => setUsername(e.target.value)}
            placeholder="Username or email"
            autoComplete="username"
            required
          />
        </label>
        <label className="input-wrapper">
          <span className="input-icon" aria-hidden="true">
            *
          </span>
          <input
            type={showPassword ? 'text' : 'password'}
            name="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            placeholder="Password"
            autoComplete="current-password"
            required
          />
          <button
            type="button"
            className="input-addon"
            aria-label="Toggle password visibility"
            onClick={() => setShowPassword((prev) => !prev)}
          >
            {showPassword ? 'Hide' : 'Show'}
          </button>
        </label>
        {error && (
          <p className="login-error" role="alert">
            {error}
          </p>
        )}
        <div className="remember-row">
          <label className="remember-checkbox">
            <input type="checkbox" defaultChecked />
            <span>Remember me</span>
          </label>
          <button type="button" onClick={onForgot}>
            Forgot password?
          </button>
        </div>
        <button type="submit" className="login-btn" disabled={isLoading}>
          {isLoading ? 'Logging in...' : 'Log In'}
        </button>
        <div className="divider">
          <span />
          <p>OR</p>
          <span />
        </div>
        <div className="social-login">
          {[
            { id: 'apple', label: 'Log in with Apple', icon: 'A' },
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
}

export default LoginPanel
