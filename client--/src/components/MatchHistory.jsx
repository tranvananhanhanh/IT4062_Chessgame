import React, { useEffect, useState } from 'react';
import { History } from 'lucide-react';
import api from '../api/api';

const MatchHistory = ({ token, onError }) => {
    const [history, setHistory] = useState([]);
    const [isLoading, setIsLoading] = useState(true);

    useEffect(() => {
        const fetchHistory = async () => {
            try {
                const data = await api.getHistory(token);
                setHistory(data);
            } catch (error) {
                onError(error.message);
            } finally {
                setIsLoading(false);
            }
        };
        fetchHistory();
    }, [token, onError]);

    if (isLoading) return <div className="text-center p-4">Đang tải lịch sử...</div>;

    return (
        <div className="bg-white p-4 rounded-xl shadow-inner h-full overflow-y-auto">
            <h3 className="text-xl font-bold mb-4 flex items-center text-gray-700">
                <History className="mr-2" size={20} /> Lịch Sử Đối Đầu AI
            </h3>
            {history.length === 0 ? (
                <p className="text-gray-500 italic">Chưa có trận đấu nào được lưu.</p>
            ) : (
                <ul className="space-y-2">
                    {history.map((match) => (
                        <li key={match.id} className="p-3 border-b last:border-b-0 flex justify-between items-center hover:bg-indigo-50/50 rounded transition-colors">
                            <div className="text-gray-800">
                                <span className="font-medium">{match.date}</span> - vs {match.opponent}
                            </div>
                            <span className={`px-3 py-1 text-xs font-bold rounded-full ${match.result === 'Thắng' ? 'bg-green-100 text-green-800' : match.result === 'Thua' ? 'bg-red-100 text-red-800' : 'bg-gray-100 text-gray-800'}`}>
                                {match.result}
                            </span>
                        </li>
                    ))}
                </ul>
            )}
        </div>
    );
};

export default MatchHistory;
