const ControlPanel = ({
  controlTab,
  setControlTab,
  history,
  onResetGame,
  onPlayOnline,
  onClearHistory,
}) => {
  return (
    <div className="control-column">
      <div className="control-tabs">
        {[
          { id: 'play', label: 'Chơi' },
          { id: 'newGame', label: 'Ván cờ mới' },
          { id: 'games', label: 'Các ván đấu' },
          { id: 'players', label: 'Các kỳ thủ' },
        ].map((tab) => (
          <button
            type="button"
            key={tab.id}
            className={controlTab === tab.id ? 'active' : ''}
            onClick={() => setControlTab(tab.id)}
          >
            {tab.label}
          </button>
        ))}
      </div>

      {controlTab === 'newGame' && (
        <>
          <div className="control-card accent">
            <div>
              <p className="control-label">Chế độ</p>
              <strong>10 min (Rapid)</strong>
            </div>
            <button
              type="button"
              className="control-primary"
              onClick={() => onResetGame('Bắt đầu ngay ván mới!')}
            >
              Start Game
            </button>
          </div>

          <button type="button" className="control-card" onClick={onPlayOnline}>
            ♟ Play Online
          </button>

          <button type="button" className="control-card">
            🧩 Custom Challenge
          </button>
          <button type="button" className="control-card">
            🤝 Play a Friend
          </button>
          <button type="button" className="control-card">
            🏅 Tournaments
          </button>
        </>
      )}

      {controlTab === 'play' && (
        <div className="move-panel">
          <div className="move-tabs">
            <button type="button" className="active">
              Các nước đi
            </button>
            <button type="button">Thông tin</button>
          </div>

          <div className="move-list">
            {history.length ? (
              history
                .slice()
                .reverse()
                .map((entry, index) => (
                  <div className="move-row" key={`${entry}-${index}`}>
                    <span>{index + 1}.</span>
                    <p>{entry}</p>
                  </div>
                ))
            ) : (
              <p className="muted">Chưa có nước đi nào</p>
            )}
          </div>

          <div className="move-controls">
            <button type="button" aria-label="Về đầu">
              ⏮
            </button>
            <button type="button" aria-label="Lùi">
              ⏪
            </button>
            <button type="button" aria-label="Phát">
              ▶
            </button>
            <button type="button" aria-label="Tiến">
              ⏩
            </button>
            <button type="button" aria-label="Tới cuối">
              ⏭
            </button>
          </div>

          <div className="game-summary">
            <p>Ván cờ đang diễn ra</p>
            <small>IT4062 Rapid • 10 phút</small>
            <button type="button" className="link-btn" onClick={onClearHistory}>
              Xóa lịch sử
            </button>
          </div>

          <div className="move-actions">
            <button type="button">½ Hòa cờ</button>
            <button type="button">🏳️ Đầu Hàng</button>
            <button
              type="button"
              onClick={() => onResetGame('Bắt đầu lại trận đấu!')}
            >
              ⟳ Đấu lại
            </button>
          </div>
        </div>
      )}

      {controlTab !== 'newGame' && controlTab !== 'play' && (
        <div className="control-placeholder">
          <p>
            {controlTab === 'games'
              ? 'Danh sách ván đấu sắp ra mắt.'
              : 'Tra cứu kỳ thủ sẽ xuất hiện trong bản tới.'}
          </p>
          <small>Vào tab "Ván cờ mới" để bắt đầu một trận.</small>
        </div>
      )}
    </div>
  )
}

export default ControlPanel
