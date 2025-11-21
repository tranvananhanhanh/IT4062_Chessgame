import React, { useCallback, useEffect, useState } from 'react';
import { Cpu, Grid, LogOut } from 'lucide-react';
import ChessBoard from './components/ChessBoard';
import LoginForm from './components/LoginForm';
import MatchHistory from './components/MatchHistory';
import AlertMessage from './components/AlertMessage';
import api from './api/api';
import './App.css';

const STARTING_FEN = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1';
const appId = typeof __app_id !== 'undefined' ? __app_id : 'default-app-id';

export default function App() {
  const [authState, setAuthState] = useState({ isAuthenticated: false, token: null, user: null });
  const [boardFen, setBoardFen] = useState(STARTING_FEN);
  const [gameId, setGameId] = useState(null);
  const [error, setError] = useState(null);
  const [isLoading, setIsLoading] = useState(false);
  const [message, setMessage] = useState('Sẵn sàng bắt đầu ván mới!');
  const [gameStarted, setGameStarted] = useState(true); // chỉ hiển thị bàn cờ sau khi bắt đầu

  useEffect(() => {
    setGameId('game-' + appId + Date.now());
  }, []);

  // Demo mode: ?demo=1 sẽ tự đăng nhập khách
  useEffect(() => {
    const params = new URLSearchParams(window.location.search);
    if (params.get('demo') === '1') {
      setAuthState({ isAuthenticated: true, token: 'demo-token', user: { username: 'GuestPlayer' } });
    }
  }, []);

  const handlePlayerMove = useCallback(async (move) => {
    if (!authState.isAuthenticated || isLoading) return;
    setError(null);
    setIsLoading(true);
    setMessage('Đang xử lý nước đi...');
    try {
      const result = await api.makeMove(authState.token, gameId, move);
      setBoardFen(result.new_fen);
      setMessage(`Bạn đã đi: ${move}. Chờ AI tính toán...`);
      if (result.status === 'CHECKMATE') {
        setMessage('Bạn đã chiếu hết! Chúc mừng!');
        setIsLoading(false);
        return;
      }
      if (result.ai_move) {
        setBoardFen(result.new_fen);
        setMessage(`AI đã đi: ${result.ai_move}. Lượt bạn.`);
      } else {
        setMessage('AI chưa trả về nước đi.');
      }
    } catch (e) {
      setError(e.message);
      setMessage('Lỗi xảy ra trong ván đấu.');
    } finally {
      setIsLoading(false);
    }
  }, [authState.isAuthenticated, authState.token, gameId, isLoading]);

  const handleLogout = () => {
    setAuthState({ isAuthenticated: false, token: null, user: null });
    setError(null);
    setMessage('Bạn đã đăng xuất.');
    setGameStarted(false); // ẩn bàn cờ khi đăng xuất
  };

  if (!authState.isAuthenticated) {
    return (
      <div className="min-h-screen bg-gray-100 flex items-center justify-center p-4">
        <LoginForm setAuth={setAuthState} onError={setError} />
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gray-50 p-4 sm:p-8 flex flex-col items-center">
      <h1 className="text-4xl font-extrabold text-indigo-700 mb-6 flex items-center">
        <Cpu className="mr-3" size={36} /> Cờ Vua AI
      </h1>
      <div className="w-full max-w-5xl grid grid-cols-1 lg:grid-cols-3 gap-8">
        <div className="lg:col-span-1 space-y-6">
          <div className="bg-white p-5 rounded-xl shadow-lg">
            <h2 className="text-2xl font-bold text-gray-800 mb-2">Xin chào, {authState.user?.username}</h2>
            <p className="text-sm text-gray-500 mb-4">ID Game hiện tại: <span className="font-mono text-xs p-1 bg-gray-100 rounded">{gameId}</span></p>
            <div className="flex flex-col space-y-3">
              <button
                onClick={() => {
                  setBoardFen(STARTING_FEN);
                  setGameId('game-' + appId + Date.now());
                  setMessage('Đã bắt đầu ván mới!');
                  setGameStarted(true); // hiển thị bàn cờ sau khi bắt đầu
                }}
                className="flex items-center justify-center bg-green-500 text-white p-3 rounded-xl font-semibold hover:bg-green-600 transition duration-300 shadow-md"
                disabled={isLoading}
              >
                <Grid className="mr-2" size={20} /> Bắt Đầu Ván Mới
              </button>
              <button
                onClick={handleLogout}
                className="flex items-center justify-center bg-gray-400 text-white p-3 rounded-xl font-semibold hover:bg-gray-500 transition duration-300 shadow-md"
                disabled={isLoading}
              >
                <LogOut className="mr-2" size={20} /> Đăng Xuất
              </button>
            </div>
          </div>
          <AlertMessage message={error} type="error" />
          <div className="bg-white p-5 rounded-xl shadow-lg">
            <h3 className="text-xl font-bold mb-3 text-gray-700">Trạng Thái Ván Đấu</h3>
            <p className={`p-3 rounded-lg font-mono text-sm ${isLoading ? 'bg-yellow-100 text-yellow-800' : 'bg-blue-100 text-blue-800'}`}>{message}</p>
            <p className="mt-3 text-sm text-gray-600">FEN: <span className="font-mono text-xs break-all">{boardFen}</span></p>
          </div>
        </div>

        <div className="lg:col-span-2 flex justify-center items-start">
          {gameStarted ? (
            <ChessBoard fen={boardFen} onSquareClick={handlePlayerMove} />
          ) : (
            <div className="w-full max-w-lg bg-white rounded-xl shadow-md p-6 text-center text-gray-600">
              Nhấn "Bắt Đầu Ván Mới" để hiển thị bàn cờ.
            </div>
          )}
        </div>
        <div className="lg:col-span-3 lg:col-span-1 lg:row-start-1 lg:col-start-3 max-h-96 lg:max-h-full">
          <MatchHistory token={authState.token} onError={setError} />
        </div>
      </div>
    </div>
  );
}
