import { useMemo, useState } from 'react'
import Header from './components/layout/Header'
import SideNav from './components/navigation/SideNav'
import LoginPanel from './components/auth/LoginPanel'
import RegisterPanel from './components/auth/RegisterPanel'
import IntroHero from './components/home/IntroHero'
import ChessBoardLayout from './components/board/ChessBoardLayout'
import ControlPanel from './components/game/ControlPanel'

import './App.css'

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

function App() {
  const [board, setBoard] = useState(() => createInitialBoard())
  const [selectedSquare, setSelectedSquare] = useState(null)
  const [turn, setTurn] = useState('white')
  const [history, setHistory] = useState([])
  const [captured, setCaptured] = useState({ white: [], black: [] }) // hiện chưa hiển thị nhưng giữ lại
  const [gameMessage, setGameMessage] = useState('Trắng đi trước')
  const [lastMove, setLastMove] = useState(null)
  const [gameStarted, setGameStarted] = useState(false)
  const [showLoginPanel, setShowLoginPanel] = useState(false)
  const [showRegisterPanel, setShowRegisterPanel] = useState(false)
  const [controlTab, setControlTab] = useState('newGame')

  // Match making
  const [currentUser, setCurrentUser] = useState(null)
  const [isMatching, setIsMatching] = useState(false)
  const [opponent, setOpponent] = useState(null)

  const handleRegisterClick = () => {
    setShowRegisterPanel(true)
    setGameMessage('Mở biểu mẫu đăng ký tài khoản.')
  }

  const handleRegisterSubmit = (event) => {
    event.preventDefault()
    setShowRegisterPanel(false)
    setGameMessage('Đã gửi form đăng ký (demo – chưa kết nối backend).')
  }

  const handleCloseRegister = () => setShowRegisterPanel(false)

  const handleLoginClick = () => {
    setShowLoginPanel(true)
    setGameMessage('Mở biểu mẫu đăng nhập để kết nối với cộng đồng.')
  }

  const handleLoginSubmit = (event) => {
    event.preventDefault()
    setShowLoginPanel(false)
    setGameMessage('Đăng nhập demo thành công, nhấn Get Started để vào bàn cờ.')
    // TODO: sau khi nối backend login, setCurrentUser(user) ở đây
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

  const handlePlayOnline = () => {
    // TODO: sau này có thể bắt buộc đăng nhập:
    // if (!currentUser) { setGameMessage('Hãy đăng nhập để chơi online.'); return }

    if (!gameStarted) {
      setGameStarted(true)
      resetGame('Chuẩn bị ván cờ online, đang tìm đối thủ...')
    }

    setIsMatching(true)
    setOpponent(null)
    setGameMessage('Đang tìm đối thủ có ELO phù hợp...')

    // TODO: sau này thay bằng API /api/match/join
    setTimeout(() => {
      const fakeOpponent = { name: 'Đối thủ demo', elo: 1010 }
      setIsMatching(false)
      setOpponent(fakeOpponent)
      setGameMessage(`Đã tìm thấy ${fakeOpponent.name}, ván cờ bắt đầu!`)
      resetGame(`Ván cờ online với ${fakeOpponent.name}`)
    }, 2000)
  }

  const handleGetStarted = () => {
    setGameStarted(true)
    resetGame('Bắt đầu ngay một ván mới và khám phá hệ thống IT4062 Chessgame.')
  }

  return (
    <div className="chess-shell">
      <SideNav onLoginClick={handleLoginClick} onRegisterClick={handleRegisterClick} />
      <main className="main-area">
        <Header />

        {!gameStarted && (
          <IntroHero
            gameMessage={gameMessage}
            onGetStarted={handleGetStarted}
            onLearnMore={handleLearnMore}
          />
        )}

        {gameStarted && (
          <section className="play-section">
            <ChessBoardLayout
              board={board}
              selectedSquare={selectedSquare}
              validMoves={validMoves}
              lastMove={lastMove}
              onSquareClick={handleSquareClick}
              opponent={opponent}
              isMatching={isMatching}
              currentUser={currentUser}
              describePiece={describePiece}
              toNotation={toNotation}
              pieceSymbols={PIECE_SYMBOLS}
            />

            <ControlPanel
              controlTab={controlTab}
              setControlTab={setControlTab}
              history={history}
              onResetGame={resetGame}
              onPlayOnline={handlePlayOnline}
              onClearHistory={() => setHistory([])}
            />
          </section>
        )}
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

      {showRegisterPanel && (
        <div className="login-overlay">
          <RegisterPanel onSubmit={handleRegisterSubmit} onClose={handleCloseRegister} />
        </div>
      )}
    </div>
  )
}

export default App
