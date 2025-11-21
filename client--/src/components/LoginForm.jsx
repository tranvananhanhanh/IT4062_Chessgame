import React, { useState } from 'react';
import { LogIn } from 'lucide-react';
import api from '../api/api';

const LoginForm = ({ setAuth, onError }) => {
    const [username, setUsername] = useState('');
    const [password, setPassword] = useState('');
    const [isRegister, setIsRegister] = useState(false);
    const [isLoading, setIsLoading] = useState(false);

    const handleSubmit = async (e) => {
        e.preventDefault();
        setIsLoading(true);
        onError(null);
        try {
            const data = await api.login(username, password);
            setAuth({ token: data.token, user: data.user, isAuthenticated: true });
        } catch (error) {
            onError(error.message);
        } finally {
            setIsLoading(false);
        }
    };

    return (
        <form onSubmit={handleSubmit} className="p-6 bg-white rounded-2xl shadow-xl w-full max-w-sm">
            <h2 className="text-3xl font-bold mb-6 text-center text-gray-800 flex items-center justify-center">
                <LogIn className="mr-3 text-indigo-600" size={30} />
                {isRegister ? 'Đăng Ký' : 'Đăng Nhập'}
            </h2>
            <div className="space-y-4">
                <input
                    type="text"
                    placeholder="Tên người dùng"
                    value={username}
                    onChange={(e) => setUsername(e.target.value)}
                    required
                    className="w-full p-3 border border-gray-300 rounded-lg focus:ring-indigo-500 focus:border-indigo-500 transition duration-150"
                />
                <input
                    type="password"
                    placeholder="Mật khẩu"
                    value={password}
                    onChange={(e) => setPassword(e.target.value)}
                    required
                    className="w-full p-3 border border-gray-300 rounded-lg focus:ring-indigo-500 focus:border-indigo-500 transition duration-150"
                />
            </div>
            <button
                type="submit"
                disabled={isLoading}
                className="w-full mt-6 bg-indigo-600 text-white p-3 rounded-lg font-semibold hover:bg-indigo-700 transition duration-300 shadow-md disabled:opacity-50"
            >
                {isLoading ? 'Đang xử lý...' : isRegister ? 'Đăng Ký Tài Khoản' : 'Đăng Nhập'}
            </button>
            <button
                type="button"
                onClick={() => setIsRegister(!isRegister)}
                className="w-full mt-3 text-indigo-600 text-sm hover:text-indigo-800 transition duration-300"
            >
                {isRegister ? 'Đã có tài khoản? Đăng nhập' : 'Chưa có tài khoản? Đăng ký'}
            </button>
        </form>
    );
};

export default LoginForm;
