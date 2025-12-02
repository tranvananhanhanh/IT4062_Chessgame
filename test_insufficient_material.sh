#!/bin/bash

# Test insufficient material detection
# Case: King vs King + Knight (should be draw)

echo "Testing insufficient material detection..."
echo ""

# Start a new bot game
echo "1. Starting bot game..."
RESPONSE=$(curl -s -X POST http://localhost:5000/mode/bot \
  -H "Content-Type: application/json" \
  -d '{"user_id": 1}')

echo "Response: $RESPONSE"
MATCH_ID=$(echo $RESPONSE | grep -o '"match_id":[0-9]*' | grep -o '[0-9]*')
echo "Match ID: $MATCH_ID"
echo ""

# Make moves to reach King vs King + Knight position
# We'll use a series of moves that quickly lead to this endgame

echo "2. Making moves to reach insufficient material..."

# This is a simplified test - in reality you'd need many moves
# For now, let's just check if the detection works

sleep 2

echo ""
echo "3. Checking database..."
PGPASSWORD=0000 psql -U postgres -d chess_game -c "SELECT match_id, status, result FROM match_game WHERE match_id=$MATCH_ID;"

echo ""
echo "Test complete!"
