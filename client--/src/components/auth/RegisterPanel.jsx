import { useState } from 'react'

const RegisterPanel = ({ onSubmit, onClose, isLoading = false, error = '' }) => {
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [confirm, setConfirm] = useState('')

  const handleSubmit = (event) => {
    event.preventDefault()
    if (password !== confirm) return
    onSubmit?.({ username, password })
  }

  return (
    <div className="login-panel">
      <button type="button" className="close-login" aria-label="Close register" onClick={onClose}>
        X
      </button>
      <form className="login-form" onSubmit={handleSubmit}>
        <h3>Dang ky tai khoan</h3>
        <label className="input-wrapper">
          <span className="input-icon" aria-hidden="true">
            @
          </span>
          <input
            type="text"
            name="username"
            value={username}
            onChange={(e) => setUsername(e.target.value)}
            placeholder="Username"
            autoComplete="username"
            required
          />
        </label>
        <label className="input-wrapper">
          <span className="input-icon" aria-hidden="true">
            *
          </span>
          <input
            type="password"
            name="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            placeholder="Password"
            autoComplete="new-password"
            required
          />
        </label>
        <label className="input-wrapper">
          <span className="input-icon" aria-hidden="true">
            *
          </span>
          <input
            type="password"
            name="confirm"
            value={confirm}
            onChange={(e) => setConfirm(e.target.value)}
            placeholder="Confirm password"
            autoComplete="new-password"
            required
          />
        </label>
        {password !== confirm && confirm !== '' && (
          <p className="login-error" role="alert">
            Mat khau khong khop
          </p>
        )}
        {error && (
          <p className="login-error" role="alert">
            {error}
          </p>
        )}
        <button type="submit" className="login-btn" disabled={isLoading || password !== confirm}>
          {isLoading ? 'Dang tao...' : 'Dang ky'}
        </button>
      </form>
    </div>
  )
}

export default RegisterPanel
