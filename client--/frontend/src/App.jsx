import { useState } from 'react';
import ChessGame from './components/ChessGame';
import MatchHistory from './components/MatchHistory';
import './App.css';

function App() {
    const [activeTab, setActiveTab] = useState('game');

    return (
        <div className="App">
            <nav className="app-nav">
                <div className="nav-tabs">
                    <button
                        className={`nav-tab ${activeTab === 'game' ? 'active' : ''}`}
                        onClick={() => setActiveTab('game')}
                    >
                        ♟️ Play Game
                    </button>
                    <button
                        className={`nav-tab ${activeTab === 'history' ? 'active' : ''}`}
                        onClick={() => setActiveTab('history')}
                    >
                        📊 Match History
                    </button>
                </div>
            </nav>

            <main className="app-main">
                {activeTab === 'game' && <ChessGame />}
                {activeTab === 'history' && <MatchHistory />}
            </main>

            <footer className="app-footer">
                <p>♔ Chess Game System - IT4062 ♚</p>
                <p>Built with React, Flask, and C</p>
            </footer>
        </div>
    );
}

export default App;