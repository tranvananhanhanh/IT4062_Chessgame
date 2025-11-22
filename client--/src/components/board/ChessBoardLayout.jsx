const FILES = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h']

const ChessBoardLayout = ({
  board,
  selectedSquare,
  validMoves,
  lastMove,
  onSquareClick,
  opponent,
  isMatching,
  currentUser,
  describePiece,
  toNotation,
  pieceSymbols,
}) => {
  return (
    <div className="board-column">
      {/* Đấu thủ (trên cùng) */}
      <div className="player-row opponent-row">
        <div className="player-avatar" aria-hidden="true" />
        <div className="player-info">
          <p className="player-label">Đấu thủ</p>
          <p className="player-name">
            {isMatching ? 'Đang tìm kiếm...' : opponent?.name || 'Chưa có đối thủ'}
          </p>
          {!isMatching && opponent && <p className="player-elo">ELO {opponent.elo}</p>}
        </div>
      </div>

      <div className="play-top">
        <span className="play-mode">10 min Rapid</span>
        <div className="play-settings">
          <span>⚙️</span>
          <span className="play-timer">10:00</span>
        </div>
      </div>

      <div className="board-wrapper">
        <div className="board-shell">
          <div className="board">
            {board.map((row, rowIndex) => (
              <div className="board-row" key={`row-${rowIndex}`}>
                {row.map((piece, colIndex) => {
                  const isDark = (rowIndex + colIndex) % 2 === 1
                  const isSelected =
                    selectedSquare &&
                    selectedSquare.row === rowIndex &&
                    selectedSquare.col === colIndex
                  const moveHighlight = validMoves.find(
                    (m) => m.row === rowIndex && m.col === colIndex
                  )
                  const isLastMoveSquare =
                    lastMove &&
                    ((lastMove.from.row === rowIndex && lastMove.from.col === colIndex) ||
                      (lastMove.to.row === rowIndex && lastMove.to.col === colIndex))

                  const squareClasses = [
                    'square',
                    isDark ? 'square-dark' : 'square-light',
                    isSelected ? 'square-selected' : '',
                    moveHighlight
                      ? moveHighlight.capture
                        ? 'square-capture'
                        : 'square-move'
                      : '',
                    isLastMoveSquare ? 'square-last' : '',
                  ]
                    .filter(Boolean)
                    .join(' ')

                  const symbol = piece ? pieceSymbols[piece.color][piece.type] : ''
                  const ariaLabel = piece
                    ? `${describePiece(piece)} tại ${toNotation(rowIndex, colIndex)}`
                    : `Ô trống ${toNotation(rowIndex, colIndex)}`

                  return (
                    <button
                      type="button"
                      key={`col-${colIndex}`}
                      className={squareClasses}
                      onClick={() => onSquareClick(rowIndex, colIndex)}
                      aria-label={ariaLabel}
                    >
                      {symbol}
                      {moveHighlight && !moveHighlight.capture && <span className="dot" />}
                    </button>
                  )
                })}
              </div>
            ))}
          </div>

          <div className="rank-labels rank-left">
            {Array.from({ length: 8 }, (_, idx) => 8 - idx).map((rank) => (
              <span key={`left-${rank}`}>{rank}</span>
            ))}
          </div>
          <div className="rank-labels rank-right">
            {Array.from({ length: 8 }, (_, idx) => 8 - idx).map((rank) => (
              <span key={`right-${rank}`}>{rank}</span>
            ))}
          </div>
          <div className="file-labels file-bottom">
            {FILES.map((file) => (
              <span key={`bottom-${file}`}>{file.toUpperCase()}</span>
            ))}
          </div>
          <div className="file-labels file-top">
            {FILES.map((file) => (
              <span key={`top-${file}`}>{file.toUpperCase()}</span>
            ))}
          </div>
        </div>

        {/* Người chơi (dưới) */}
        <div className="player-row self-row">
          <div className="player-avatar self-avatar" aria-hidden="true" />
          <div className="player-info">
            <p className="player-label">Người chơi</p>
            <p className="player-name">{currentUser ? currentUser.name : 'Bạn'}</p>
            <p className="player-elo">
              ELO {currentUser?.elo_point ?? currentUser?.elo ?? 1000}
            </p>
          </div>
        </div>
      </div>
    </div>
  )
}

export default ChessBoardLayout
