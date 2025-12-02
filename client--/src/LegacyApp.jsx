import { useMemo, useState } from 'react'
import Header from './components/layout/Header'
import SideNav from './components/navigation/SideNav'
import LoginPanel from './components/auth/LoginPanel'
import MatchHistory from './components/MatchHistory'
import ChessGameBot from './components/playbot/ChessGameBot'
import './LegacyApp.css'

const PIECE_ORDER = ['rook', 'knight', 'bishop', 'queen', 'king', 'bishop', 'knight', 'rook']

const PIECE_SYMBOLS = {
  white: {
    king: '♔',
    queen: '♕',
    rook: '♖',
    bishop: '♗',
    knight: '♘',
    pawn: '♙',
  },
  black: {
    king: '♚',
    queen: '♛',
    rook: '♜',
    bishop: '♝',
    knight: '♞',
    pawn: '♟︎',
  },
}

const PIECE_NAMES = {
  king: 'Vua',
  queen: 'Hậu',
  rook: 'Xe',
  bishop: 'Tượng',
  knight: 'Mã',
  pawn: 'Tốt',
}

const FILES = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h']

const createInitialBoard = () => {
  const board = Array.from({ length: 8 }, () => Array(8).fill(null))

  board[0] = PIECE_ORDER.map((type) => ({ type, color: 'black', hasMoved: false }))
  board[1] = Array.from({ length: 8 }, () => ({ type: 'pawn', color: 'black', hasMoved: false }))
  board[6] = Array.from({ length: 8 }, () => ({ type: 'pawn', color: 'white', hasMoved: false }))
  board[7] = PIECE_ORDER.map((type) => ({ type, color: 'white', hasMoved: false }))

  return board
}

const cloneBoard = (board) =>
  board.map((row) => row.map((cell) => (cell ? { ...cell } : null)))

const toNotation = (row, col) => `${FILES[col]}${8 - row}`

const describePiece = (piece) =>
  `${piece.color === 'white' ? 'Trắng' : 'Đen'} ${PIECE_NAMES[piece.type]}`

const capitalizeColor = (color) => (color === 'white' ? 'Trắng' : 'Đen')

const formatMoveText = (piece, from, to, isCapture) => {
  const action = isCapture ? 'x' : '→'
  return `${capitalizeColor(piece.color)} ${PIECE_NAMES[piece.type]} ${toNotation(
    from.row,
    from.col
  )} ${action} ${toNotation(to.row, to.col)}`
}

const getValidMoves = (board, row, col) => {
  const piece = board[row][col]
  if (!piece) return []

  const moves = []
  const inBounds = (r, c) => r >= 0 && r < 8 && c >= 0 && c < 8

  const addSlidingMoves = (directions) => {
    directions.forEach(([dr, dc]) => {
      let nextRow = row + dr
      let nextCol = col + dc
      while (inBounds(nextRow, nextCol)) {
        const target = board[nextRow][nextCol]
        if (!target) {
          moves.push({ row: nextRow, col: nextCol, capture: false })
        } else {
          if (target.color !== piece.color) {
            moves.push({ row: nextRow, col: nextCol, capture: true })
          }
          break
        }
        nextRow += dr
        nextCol += dc
      }
    })
  }

  switch (piece.type) {
    case 'pawn': {
      const direction = piece.color === 'white' ? -1 : 1
      const startRow = piece.color === 'white' ? 6 : 1
      const oneStep = row + direction
      if (inBounds(oneStep, col) && !board[oneStep][col]) {
        moves.push({ row: oneStep, col, capture: false })
        const twoStep = row + direction * 2
        if (row === startRow && inBounds(twoStep, col) && !board[twoStep][col]) {
          moves.push({ row: twoStep, col, capture: false })
        }
      }
      ;[-1, 1].forEach((offset) => {
        const targetCol = col + offset
        const targetRow = row + direction
        if (!inBounds(targetRow, targetCol)) return
        const target = board[targetRow][targetCol]
        if (target && target.color !== piece.color) {
          moves.push({ row: targetRow, col: targetCol, capture: true })
        }
      })
      break
    }
    case 'rook':
      addSlidingMoves([
        [1, 0],
        [-1, 0],
        [0, 1],
        [0, -1],
      ])
      break
    case 'bishop':
      addSlidingMoves([
        [1, 1],
        [1, -1],
        [-1, 1],
        [-1, -1],
      ])
      break
    case 'queen':
      addSlidingMoves([
        [1, 0],
        [-1, 0],
        [0, 1],
        [0, -1],
        [1, 1],
        [1, -1],
        [-1, 1],
        [-1, -1],
      ])
      break
    case 'knight': {
      const knightMoves = [
        [2, 1],
        [2, -1],
        [-2, 1],
        [-2, -1],
        [1, 2],
        [1, -2],
        [-1, 2],
        [-1, -2],
      ]
      knightMoves.forEach(([dr, dc]) => {
        const nextRow = row + dr
        const nextCol = col + dc
        if (!inBounds(nextRow, nextCol)) return
        const target = board[nextRow][nextCol]
        if (!target) {
          moves.push({ row: nextRow, col: nextCol, capture: false })
        } else if (target.color !== piece.color) {
          moves.push({ row: nextRow, col: nextCol, capture: true })
        }
      })
      break
    }
    case 'king': {
      const kingMoves = [
        [1, 0],
        [-1, 0],
        [0, 1],
        [0, -1],
        [1, 1],
        [1, -1],
        [-1, 1],
        [-1, -1],
      ]
      kingMoves.forEach(([dr, dc]) => {
        const nextRow = row + dr
        const nextCol = col + dc
        if (!inBounds(nextRow, nextCol)) return
        const target = board[nextRow][nextCol]
        if (!target) {
          moves.push({ row: nextRow, col: nextCol, capture: false })
        } else if (target.color !== piece.color) {
          moves.push({ row: nextRow, col: nextCol, capture: true })
        }
      })
      break
    }
    default:
      break
  }

  return moves
}

function LegacyApp() {
  const [board, setBoard] = useState(() => createInitialBoard())
  const [selectedSquare, setSelectedSquare] = useState(null)
  const [turn, setTurn] = useState('white')
  const [history, setHistory] = useState([])
  const [captured, setCaptured] = useState({ white: [], black: [] })
  const [gameMessage, setGameMessage] = useState('Trắng đi trước')
  const [lastMove, setLastMove] = useState(null)
  const [gameStarted, setGameStarted] = useState(false)
  const [showLoginPanel, setShowLoginPanel] = useState(false)
  const [controlTab, setControlTab] = useState('newGame')
  const [, setShowBoardModal] = useState(false)
  const [showHistory, setShowHistory] = useState(false)
  const [activeTab, setActiveTab] = useState('play')
  const handleRegisterClick = () =>
    setGameMessage('Đăng ký tài khoản đang được chuẩn bị cho bản client tiếp theo.')
  const handleLoginClick = () => {
    setShowLoginPanel(true)
    setGameMessage('Mở biểu mẫu đăng nhập để kết nối với cộng đồng.')
  }
  const handleLoginSubmit = (event) => {
    event.preventDefault()
    setShowLoginPanel(false)
    setGameMessage('Đăng nhập demo thành công, nhấn Get Started để vào bàn cờ.')
  }
  const handleForgotPassword = () =>
    setGameMessage('Chức năng quên mật khẩu sẽ xuất hiện trong bản dựng tiếp theo.')
  const handleCloseLogin = () => setShowLoginPanel(false)
  const handleLearnMore = () =>
    setGameMessage('Khám phá cộng đồng và các chế độ luyện tập mới của IT4062 Chessgame.')

  const validMoves = useMemo(() => {
    if (!selectedSquare) return []
    const { row, col } = selectedSquare
    const piece = board[row][col]
    if (!piece || piece.color !== turn) return []
    return getValidMoves(board, row, col)
  }, [board, selectedSquare, turn])

  const handleSquareClick = (row, col) => {
    const piece = board[row][col]

    if (selectedSquare && selectedSquare.row === row && selectedSquare.col === col) {
      setSelectedSquare(null)
      setGameMessage('Đã bỏ chọn quân cờ')
      return
    }

    if (selectedSquare) {
      const move = validMoves.find((m) => m.row === row && m.col === col)
      if (move) {
        movePiece(selectedSquare, { row, col })
        return
      }
      if (piece && piece.color === turn) {
        setSelectedSquare({ row, col })
        setGameMessage(`Chọn ô đến cho ${describePiece(piece)}`)
      } else {
        setSelectedSquare(null)
        setGameMessage('Nước đi không hợp lệ')
      }
      return
    }

    if (piece && piece.color === turn) {
      setSelectedSquare({ row, col })
      setGameMessage(`Chọn ô đến cho ${describePiece(piece)}`)
    } else if (piece) {
      setGameMessage('Đây là quân của đối thủ')
    } else {
      setGameMessage('Hãy chọn một quân cờ của bạn')
    }
  }

  const movePiece = (from, to) => {
    const movingPiece = board[from.row][from.col]
    if (!movingPiece) return
    const targetPiece = board[to.row][to.col]
    const updatedBoard = cloneBoard(board)
    updatedBoard[to.row][to.col] = { ...movingPiece, hasMoved: true }
    updatedBoard[from.row][from.col] = null

    setBoard(updatedBoard)

    if (targetPiece) {
      setCaptured((prev) => ({
        ...prev,
        [movingPiece.color]: [...prev[movingPiece.color], targetPiece],
      }))
    }

    const moveText = formatMoveText(movingPiece, from, to, Boolean(targetPiece))
    setHistory((prev) => [moveText, ...prev].slice(0, 16))
    const nextTurn = movingPiece.color === 'white' ? 'black' : 'white'
    setTurn(nextTurn)
    setSelectedSquare(null)
    setLastMove({ from, to })
    setGameMessage(
      targetPiece
        ? `${capitalizeColor(movingPiece.color)} vừa ăn ${describePiece(targetPiece)}`
        : `Đến lượt ${capitalizeColor(nextTurn)}`
    )
  }

  const resetGame = (message = 'Trắng đi trước') => {
    setBoard(createInitialBoard())
    setSelectedSquare(null)
    setTurn('white')
    setHistory([])
    setCaptured({ white: [], black: [] })
    setGameMessage(message)
    setLastMove(null)
  }

  const handleGetStarted = () => {
    setGameStarted(true)
    resetGame('Bắt đầu ngay một ván mới và khám phá hệ thống IT4062 Chessgame.')
    setShowBoardModal(true)
  }

  return (
    <div className="chess-shell">
      <SideNav
        onLoginClick={handleLoginClick}
        onRegisterClick={handleRegisterClick}
        onHistoryClick={() => setShowHistory(true)}
        onPlayBotsClick={() => setActiveTab('bot')}
      />
      <main className="main-area">
        <Header />
        {activeTab === 'play' && (
          <>
            {!gameStarted && (
              <>
                <section className="hero-section">
                  <div className="hero-text">
                    <p className="hero-label">IT4062 chess network</p>
                    <h1>Play chess. Improve your game. Have fun!</h1>
                    <p>
                      Đăng nhập để tìm đối thủ tức thì, luyện tập các thế trận mới và theo dõi cộng
                      đồng chơi cờ năng động nhất của học phần IT4062.
                    </p>
                    <div className="hero-actions">
                      <button type="button" className="primary-cta" onClick={handleGetStarted}>
                        Get Started
                      </button>
                      <button type="button" className="ghost-cta" onClick={handleLearnMore}>
                        Learn More
                      </button>
                    </div>
                  </div>
                  <div className="hero-visual" aria-hidden="true">
                    <div className="hero-floor" />
                    <div className="hero-piece hero-king" />
                    <div className="hero-piece hero-knight" />
                    <div className="hero-piece hero-pawn" />
                  </div>
                </section>

                <section className="prestart-panel">
                  <h2>Sẵn sàng tranh tài</h2>
                  <p>Tạo tài khoản hoặc đăng nhập để lưu lại lịch sử và thống kê nước đi của bạn.</p>
                  <p>Nhấn Get Started để mở ngay một bàn cờ thử nghiệm.</p>
                  <div className="prestart-status">{gameMessage}</div>
                </section>
              </>
            )}

            {gameStarted && (
              <section className="play-section">
                <div className="board-column">
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
                                ((lastMove.from.row === rowIndex &&
                                  lastMove.from.col === colIndex) ||
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
                              const symbol = piece ? PIECE_SYMBOLS[piece.color][piece.type] : ''
                              const ariaLabel = piece
                                ? `${describePiece(piece)} tại ${toNotation(rowIndex, colIndex)}`
                                : `Ô trống ${toNotation(rowIndex, colIndex)}`
                              return (
                                <button
                                  type="button"
                                  key={`col-${colIndex}`}
                                  className={squareClasses}
                                  onClick={() => handleSquareClick(rowIndex, colIndex)}
                                  aria-label={ariaLabel}
                                >
                                  {symbol}
                                  {moveHighlight && !moveHighlight.capture && (
                                    <span className="dot" />
                                  )}
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
                  </div>
                </div>
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
                          onClick={() => resetGame('Bắt đầu ngay ván mới!')}
                        >
                          Start Game
                        </button>
                      </div>
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
                        <button type="button" className="link-btn" onClick={() => setHistory([])}>
                          Xóa lịch sử
                        </button>
                      </div>
                      <div className="move-actions">
                        <button type="button">½ Hòa cờ</button>
                        <button type="button">🏳️ Đầu Hàng</button>
                        <button type="button" onClick={() => resetGame('Bắt đầu lại trận đấu!')}>
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
              </section>
            )}
          </>
        )}
        {activeTab === 'bot' && <ChessGameBot />}
      </main>
      {showLoginPanel && (
        <div className="login-overlay">
          <LoginPanel
            onSubmit={handleLoginSubmit}
            onForgot={handleForgotPassword}
            onClose={handleCloseLogin}
          />
        </div>
      )}
      {showHistory && (
        <div className="history-overlay">
          <div className="history-dialog">
            <div className="history-header">
              <div>
                <p className="history-eyebrow">More</p>
                <h3>Lịch sử chơi (Frontend API mới)</h3>
              </div>
              <button
                type="button"
                className="history-close"
                aria-label="Đóng lịch sử"
                onClick={() => setShowHistory(false)}
              >
                ×
              </button>
            </div>
            <div className="history-content">
              <MatchHistory />
            </div>
          </div>
        </div>
      )}
    </div>
  )
}

export default LegacyApp
