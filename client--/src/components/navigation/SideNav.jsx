const baseNavItems = (onHistoryClick) => [
  {
    label: 'Play',
    icon: '♟️',
    submenu: [
      { label: 'Play', icon: '♟️' },
      { label: 'Play Bots', icon: '🤖' },
      { label: 'Play Coach', icon: '🧠' },
      { label: 'Tournaments', icon: '🏅' },
      { label: '4 Player & Variants', icon: '🎲' },
      { label: 'Leaderboard', icon: '📊' },
    ],
  },
  { label: 'Puzzles', icon: '🧩' },
  { label: 'Learn', icon: '📘' },
  { label: 'Watch', icon: '▶️' },
  { label: 'News', icon: '📰' },
  { label: 'Social', icon: '👥' },
  {
    label: 'More',
    icon: '⋯',
    submenu: [
      { label: 'Lịch sử chơi (Frontend API mới)', icon: '📜', onClick: onHistoryClick },
    ],
  },
]

const SideNav = ({ user, onLoginClick, onRegisterClick, onHistoryClick, onLogout }) => (
  <aside className="side-nav">
    <div className="nav-brand">
      <span className="brand-icon">♙</span>
      <div>
        <strong>Chessgame</strong>
        <p>by IT4062</p>
      </div>
    </div>

    <ul className="nav-menu">
      {baseNavItems(onHistoryClick).map((item) => (
        <li key={item.label} className={item.submenu ? 'has-flyout' : ''}>
          <button type="button">
            <span className="item-icon" aria-hidden="true">
              {item.icon}
            </span>
            {item.label}
          </button>
          {item.submenu && (
            <div className="nav-flyout">
              <ul>
                {item.submenu.map((child) => (
                  <li key={child.label}>
                    <button type="button" onClick={child.onClick}>
                      <span aria-hidden="true">{child.icon}</span>
                      {child.label}
                    </button>
                  </li>
                ))}
              </ul>
            </div>
          )}
        </li>
      ))}
    </ul>

    <div className="nav-search">
      <input type="text" placeholder="Search" aria-label="Tìm kiếm" />
      <span className="nav-search-icon">🔍</span>
    </div>

    <div className="nav-cta">
      {user ? (
        <div className="nav-profile">
          <div className="profile-avatar" aria-hidden="true">
            {(user.username || '?').charAt(0).toUpperCase()}
          </div>
          <div className="profile-meta">
            <strong>{user.username}</strong>
            <p>ELO {user.elo ?? '-'}</p>
          </div>
          <button type="button" className="profile-action" onClick={onLogout}>
            Sign out
          </button>
        </div>
      ) : (
        <>
          <button type="button" className="sign-up-btn" onClick={onRegisterClick}>
            Sign Up
          </button>
          <button type="button" className="log-in-btn" onClick={onLoginClick}>
            Log In
          </button>
        </>
      )}
    </div>

    <div className="nav-footer">
      <button type="button">🌐 English</button>
      <button type="button">❓ Support</button>
    </div>
  </aside>
)

export default SideNav
