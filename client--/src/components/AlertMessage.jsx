import React from 'react';

const AlertMessage = ({ message, type = 'error' }) => {
    if (!message) return null;
    const color = type === 'error' ? 'bg-red-500' : 'bg-green-500';
    return (
        <div className={`p-3 rounded-xl text-white font-semibold ${color} mb-4 shadow-lg`} role="alert">
            {message}
        </div>
    );
};

export default AlertMessage;
