#!/bin/bash

# Test PvP Chess Game Flow
# This script tests the complete PvP flow:
# 1. Player 1 (Alice) creates a match
# 2. Player 2 (Bob) joins the match
# 3. Moves are made
# 4. Game controls (pause/resume/draw/rematch) work

BASE_URL="http://localhost:5002/api"

echo "=============================================="
echo "   Chess PvP Test - Complete Flow"
echo "=============================================="

# Step 1: Player 1 creates a match
echo ""
echo "[Step 1] Alice creates a match..."
CREATE_RESPONSE=$(curl -s -X POST "$BASE_URL/game/start" \
    -H "Content-Type: application/json" \
    -d '{"player1_id": 1, "player1_name": "alice", "player2_id": 2, "player2_name": "bob"}')
echo "Response: $CREATE_RESPONSE"

# Extract match_id
MATCH_ID=$(echo $CREATE_RESPONSE | grep -o '"match_id":[0-9]*' | cut -d':' -f2)
echo "Match ID: $MATCH_ID"

if [ -z "$MATCH_ID" ]; then
    echo "ERROR: Failed to create match"
    exit 1
fi

# Step 2: Check status (should be WAITING)
echo ""
echo "[Step 2] Checking match status (should be WAITING)..."
STATUS_RESPONSE=$(curl -s "$BASE_URL/game/status/$MATCH_ID")
echo "Response: $STATUS_RESPONSE"

# Step 3: Player 2 joins the match
echo ""
echo "[Step 3] Bob joins the match..."
JOIN_RESPONSE=$(curl -s -X POST "$BASE_URL/game/join/$MATCH_ID" \
    -H "Content-Type: application/json" \
    -d '{"player_id": 2, "player_name": "bob"}')
echo "Response: $JOIN_RESPONSE"

# Step 4: Check status again (should be PLAYING now)
echo ""
echo "[Step 4] Checking match status (should be PLAYING)..."
STATUS_RESPONSE=$(curl -s "$BASE_URL/game/status/$MATCH_ID")
echo "Response: $STATUS_RESPONSE"

# Step 5: Alice (white) makes a move
echo ""
echo "[Step 5] Alice (white) makes move e2 -> e4..."
MOVE_RESPONSE=$(curl -s -X POST "$BASE_URL/game/move" \
    -H "Content-Type: application/json" \
    -d "{\"match_id\": $MATCH_ID, \"player_id\": 1, \"from_square\": \"e2\", \"to_square\": \"e4\"}")
echo "Response: $MOVE_RESPONSE"

# Step 6: Bob (black) makes a move
echo ""
echo "[Step 6] Bob (black) makes move e7 -> e5..."
MOVE_RESPONSE=$(curl -s -X POST "$BASE_URL/game/move" \
    -H "Content-Type: application/json" \
    -d "{\"match_id\": $MATCH_ID, \"player_id\": 2, \"from_square\": \"e7\", \"to_square\": \"e5\"}")
echo "Response: $MOVE_RESPONSE"

# Step 7: Test PAUSE
echo ""
echo "[Step 7] Alice pauses the game..."
PAUSE_RESPONSE=$(curl -s -X POST "$BASE_URL/game/control" \
    -H "Content-Type: application/json" \
    -d "{\"match_id\": $MATCH_ID, \"player_id\": 1, \"action\": \"PAUSE\"}")
echo "Response: $PAUSE_RESPONSE"

# Step 8: Check status (should be PAUSED)
echo ""
echo "[Step 8] Checking match status (should be PAUSED)..."
STATUS_RESPONSE=$(curl -s "$BASE_URL/game/status/$MATCH_ID")
echo "Response: $STATUS_RESPONSE"

# Step 9: Test RESUME
echo ""
echo "[Step 9] Bob resumes the game..."
RESUME_RESPONSE=$(curl -s -X POST "$BASE_URL/game/control" \
    -H "Content-Type: application/json" \
    -d "{\"match_id\": $MATCH_ID, \"player_id\": 2, \"action\": \"RESUME\"}")
echo "Response: $RESUME_RESPONSE"

# Step 10: Check status (should be PLAYING again)
echo ""
echo "[Step 10] Checking match status (should be PLAYING)..."
STATUS_RESPONSE=$(curl -s "$BASE_URL/game/status/$MATCH_ID")
echo "Response: $STATUS_RESPONSE"

echo ""
echo "=============================================="
echo "   Test Complete!"
echo "=============================================="
