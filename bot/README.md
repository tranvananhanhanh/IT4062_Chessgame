# Chess Bot Server

## Troubleshooting: "Address already in use" Error

If you see the error:
```
[Bot] Server error: [Errno 98] Address already in use
```

This means port 5001 is still occupied from a previous run. Here are the solutions:

### Solution 1: Kill the old process (Linux/Mac/WSL)
```bash
pkill -f "bot_socket_server"
sleep 2
python3 bot_socket_server.py
```

### Solution 2: Use the provided startup script
```bash
# On Linux/Mac/WSL
bash start_bot.sh

# On Windows
start_bot.bat
```

### Solution 3: Change the port (if needed)
Edit `bot_socket_server.py` and change:
```python
PORT = 5001  # Change this to another port like 5002
```

Then update the C server code in `server-c/src/bot/bot.c`:
```c
addr.sin_port = htons(5001);  // Change to your new port
```

## Installation

1. Create virtual environment:
```bash
python3 -m venv .venv
source .venv/bin/activate  # On Windows: .venv\Scripts\activate
```

2. Install dependencies:
```bash
pip install -r requirements.txt
```

3. Run the bot server:
```bash
python3 bot_socket_server.py
```

## How it Works

The bot server:
- Listens on `127.0.0.1:5001`
- Receives FEN (chess position) + difficulty level
- Calculates the best move using python-chess
- Returns the move in UCI format (e.g., "e2e4")

Supported difficulty levels:
- `easy`: Random moves with bias towards captures
- `medium`: Random legal moves
- `hard`: Best moves using Stockfish (if available) or Minimax
