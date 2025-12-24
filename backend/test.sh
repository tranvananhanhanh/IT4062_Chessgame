#!/bin/bash

API_URL="http://localhost:5002/api"

echo "===== TEST REGISTER ====="
curl -s -X POST "http://localhost:5002/api/auth/register" -H "Content-Type: application/json" -d '{"username":"em","password":"123456"}'
echo
curl -s -X POST "http://localhost:5002/api/auth/register" -H "Content-Type: application/json" -d '{"username":"bob","password":"123456"}'
echo

echo "===== TEST LOGIN ====="
curl -s -X POST "http://localhost:5002/api/auth/login" -H "Content-Type: application/json" -d '{"username":"alice","password":"123456"}'
echo

echo "===== TEST MATCHMAKING JOIN ====="
curl -s -X POST "http://localhost:5002/matchmaking/join" -H "Content-Type: application/json" -d '{"username":"emm"}'
echo
curl -s -X POST "http://localhost:5002/matchmaking/join" -H "Content-Type: application/json" -d '{"username":"emmn"}'
echo

echo "===== TEST MATCHMAKING STATUS ====="
curl -s "http://localhost:5002/matchmaking/status?username=alice"
echo


echo "===== TEST GAME START ====="
curl -s -X POST "http://localhost:5002/api/game/start" -H "Content-Type: application/json" -d '{"player1_id":5,"player1_name":"emm","player2_id":6,"player2_name":"emmn"}'
echo

echo "===== TEST GAME MOVE ====="
curl -s -X POST "http://localhost:5002/api/game/move" -H "Content-Type: application/json" -d '{"match_id":11,"player_id":5,"from_square":"e2","to_square":"e4"}'
echo

echo "===== TEST GAME STATUS ====="
curl -s "http://localhost:5002/api/game/status/11"
echo

echo "===== TEST BOT GAME ====="
curl -s -X POST "http://localhost:5002/api/mode/bot" -H "Content-Type: application/json" -d '{"user_id":1}'
echo

echo "===== TEST BOT MOVE ====="
curl -s -X POST "http://localhost:5002/api/game/bot/move" -H "Content-Type: application/json" -d '{"match_id":4,"move":"e2e4"}'
echo

echo "===== TEST FRIEND REQUEST ====="
curl -s -X POST "http://localhost:5002/api/friend/request" -H "Content-Type: application/json" -d '{"user_id":1,"friend_id":3}'
echo

echo "===== TEST FRIEND ACCEPT ====="
curl -s -X POST "http://localhost:5002/api/friend/accept" -H "Content-Type: application/json" -d '{"user_id":3,"friend_id":1}'
echo

echo "===== TEST FRIEND LIST ====="
curl -s "http://localhost:5002/api/friend/list/3"
echo

echo "===== TEST ELO LEADERBOARD ====="
curl -s "http://localhost:5002/api/elo/leaderboard"
echo

echo "===== TEST ELO HISTORY ====="
curl -s "http://localhost:5002/api/elo/history/1"
echo

echo "===== TEST GAME HISTORY ====="
curl -s "http://localhost:5002/api/game/history/1"
echo

echo "===== TEST GAME REPLAY ====="
curl -s "http://localhost:5002/api/game/replay/1"
echo

echo "===== TEST PLAYER STATS ====="
curl -s "http://localhost:5002/api/player/stats/1"
echo

echo "===== HEALTH CHECK ====="
curl -s "$API_URL/health"
echo
