-- ===========================================
-- DATABASE: Chess Game (PostgreSQL)
-- Description: Real-time 1v1 Chess System
-- ===========================================
DROP DATABASE IF EXISTS chess_db;
CREATE DATABASE chess_db;

-- ===========================================
-- TABLE: users
-- ===========================================
CREATE TABLE users (
    user_id SERIAL PRIMARY KEY,
    name VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    state VARCHAR(20) CHECK (state IN ('offline', 'online', 'in_game')) DEFAULT 'offline',
    elo_point INT DEFAULT 1000
);

-- ===========================================
-- TABLE: password_reset
-- ===========================================
CREATE TABLE password_reset (
    reset_id SERIAL PRIMARY KEY,
    user_id INT NOT NULL,
    otp VARCHAR(10) NOT NULL,
    expires_at TIMESTAMP NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_reset_user FOREIGN KEY (user_id)
        REFERENCES users(user_id)
        ON UPDATE CASCADE
        ON DELETE CASCADE
);
-- ===========================================
-- TABLE: match_game
-- ===========================================
CREATE TABLE match_game (
    match_id SERIAL PRIMARY KEY,
    type VARCHAR(10) CHECK (type IN ('pvp', 'bot')) DEFAULT 'pvp',
    status VARCHAR(15) CHECK (status IN ('waiting', 'playing','paused', 'finished', 'aborted')) DEFAULT 'waiting',
    starttime TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    endtime TIMESTAMP NULL,
    result VARCHAR(15) CHECK (result IN ('white_win', 'black_win', 'draw', 'surrender', 'aborted')),
    winner_id INT NULL,
    CONSTRAINT fk_winner FOREIGN KEY (winner_id)
        REFERENCES users(user_id)
        ON UPDATE CASCADE
        ON DELETE SET NULL
);

-- ===========================================
-- TABLE: match_player
-- ===========================================
CREATE TABLE match_player (
    match_player_id SERIAL PRIMARY KEY,
    match_id INT NOT NULL,
    user_id INT NULL,
    color VARCHAR(10) CHECK (color IN ('white', 'black')) NOT NULL,
    score FLOAT DEFAULT 0,
    is_bot BOOLEAN DEFAULT FALSE,
    action_state VARCHAR(20) 
        CHECK (action_state IN ('normal', 'surrender', 'draw_offer', 'rematch_request')) 
        DEFAULT 'normal',

    CONSTRAINT fk_match FOREIGN KEY (match_id)
        REFERENCES match_game(match_id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    CONSTRAINT fk_user FOREIGN KEY (user_id)
        REFERENCES users(user_id)
        ON UPDATE CASCADE
        ON DELETE SET NULL,

    CONSTRAINT unique_match_color UNIQUE (match_id, color)
);

-- ===========================================
-- TABLE: move
-- ===========================================
CREATE TABLE move (
    move_id SERIAL PRIMARY KEY,
    match_id INT NOT NULL,
    user_id INT NULL,
    notation VARCHAR(20) NOT NULL,          -- ví dụ: e2e4, Nf3
    fen_after TEXT NOT NULL,                -- trạng thái bàn cờ sau nước đi
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_move_match FOREIGN KEY (match_id)
        REFERENCES match_game(match_id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    CONSTRAINT fk_move_user FOREIGN KEY (user_id)
        REFERENCES users(user_id)
        ON UPDATE CASCADE
        ON DELETE SET NULL
);

-- ===========================================
-- TABLE: friendship 
-- ===========================================
CREATE TABLE friendship (
    friendship_id SERIAL PRIMARY KEY,
    user_id INT NOT NULL,
    friend_id INT NOT NULL,
    status VARCHAR(20) CHECK (status IN ('pending', 'accepted', 'declined', 'blocked')) DEFAULT 'pending',
    requested_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    responded_at TIMESTAMP NULL,

    CONSTRAINT fk_friend_user FOREIGN KEY (user_id)
        REFERENCES users(user_id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    CONSTRAINT fk_friend_friend FOREIGN KEY (friend_id)
        REFERENCES users(user_id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    CONSTRAINT unique_friendship UNIQUE (user_id, friend_id)
);

-- ===========================================
-- INDEXES
-- ===========================================
CREATE INDEX idx_user_state ON users(state);
CREATE INDEX idx_match_status ON match_game(status);
CREATE INDEX idx_move_match ON move(match_id);
CREATE INDEX idx_move_user ON move(user_id);

-- ===========================================
-- SAMPLE DATA
-- ===========================================
INSERT INTO users (name, password_hash, state, elo_point)
VALUES 
('alice', 'hash_1', 'online', 1020),
('bob', 'hash_2', 'offline', 980);

INSERT INTO match_game (type, status, winner_id, result)
VALUES 
('pvp', 'finished', 1, 'white_win');

INSERT INTO match_player (match_id, user_id, color, score, is_bot)
VALUES
(1, 1, 'white', 1, FALSE),
(1, 2, 'black', 0, FALSE);

INSERT INTO move (match_id, user_id, notation, fen_after)
VALUES
(1, 1, 'e2e4', 'rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1'),
(1, 2, 'e7e5', 'rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2');

ALTER TABLE match_game 
DROP CONSTRAINT IF EXISTS match_game_status_check;

ALTER TABLE match_game 
ADD CONSTRAINT match_game_status_check 
CHECK (status IN ('waiting', 'playing', 'paused', 'finished', 'aborted'));
