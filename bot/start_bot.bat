@echo off
REM Start the Chess Bot Server on Windows

echo [*] Checking for existing bot processes...
taskkill /F /IM python.exe /FI "WINDOWTITLE eq bot_socket_server" 2>nul || echo No bot process found

REM Wait a moment for the port to be released
timeout /t 2 /nobreak

REM Change to bot directory
cd /d "%~dp0"

REM Activate venv if it exists
if exist ".venv\Scripts\activate.bat" (
    call .venv\Scripts\activate.bat
)

REM Run the bot server
echo [*] Starting Chess Bot Server...
python bot_socket_server.py
