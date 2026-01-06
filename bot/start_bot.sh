#!/bin/bash
# Start the Chess Bot Server

# Kill any existing bot processes on port 5001
echo "[*] Checking for existing bot processes..."
pkill -f "bot_socket_server" 2>/dev/null || true

# Wait a moment for the port to be released
sleep 2

# Start the bot server
echo "[*] Starting Chess Bot Server..."
cd "$(dirname "$0")" || exit 1

# Activate venv if it exists
if [ -d ".venv" ]; then
    source .venv/bin/activate
fi

# Run the bot server
python3 bot_socket_server.py
