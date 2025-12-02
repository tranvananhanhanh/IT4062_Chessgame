#!/bin/bash

# Test PvP flow: waiting -> playing -> rematch

echo "========================================="
echo "Test 1: Create match (should be WAITING)"
echo "========================================="

RESPONSE=$(curl -s -X POST http://localhost:5000/api/game/start \
  -H "Content-Type: application/json" \
  -d '{"player1_id": 1, "player1_name": "alice", "player2_id": 2, "player2_name": "bob"}')

echo "$RESPONSE" | jq .

MATCH_ID=$(echo "$RESPONSE" | jq -r '.match_id')
echo ""
echo "Created Match ID: $MATCH_ID"

echo ""
echo "========================================="
echo "Test 2: Check status (should be WAITING)"
echo "========================================="

sleep 1
STATUS=$(curl -s http://localhost:5000/api/game/status/$MATCH_ID)
echo "$STATUS" | jq .

echo ""
echo "========================================="
echo "Test 3: Player 2 joins (should -> PLAYING)"
echo "========================================="

sleep 1
JOIN_RESPONSE=$(curl -s -X POST http://localhost:5000/api/game/join/$MATCH_ID \
  -H "Content-Type: application/json" \
  -d '{"player_id": 2, "player_name": "bob"}')

echo "$JOIN_RESPONSE" | jq .

echo ""
echo "========================================="
echo "Test 4: Check status (should be PLAYING)"
echo "========================================="

sleep 1
STATUS=$(curl -s http://localhost:5000/api/game/status/$MATCH_ID)
echo "$STATUS" | jq .

echo ""
echo "========================================="
echo "Test 5: Make a move"
echo "========================================="

sleep 1
MOVE_RESPONSE=$(curl -s -X POST http://localhost:5000/api/game/move \
  -H "Content-Type: application/json" \
  -d "{\"match_id\": $MATCH_ID, \"player_id\": 1, \"from_square\": \"e2\", \"to_square\": \"e4\"}")

echo "$MOVE_RESPONSE" | jq .

echo ""
echo "========================================="
echo "Test 6: Surrender to finish game"
echo "========================================="

sleep 1
SURRENDER_RESPONSE=$(curl -s -X POST http://localhost:5000/api/game/surrender \
  -H "Content-Type: application/json" \
  -d "{\"match_id\": $MATCH_ID, \"player_id\": 1}")

echo "$SURRENDER_RESPONSE" | jq .

echo ""
echo "========================================="
echo "Test 7: Request rematch"
echo "========================================="

sleep 1
REMATCH_REQ=$(curl -s -X POST http://localhost:5000/api/game/control \
  -H "Content-Type: application/json" \
  -d "{\"match_id\": $MATCH_ID, \"player_id\": 1, \"action\": \"REMATCH\"}")

echo "$REMATCH_REQ" | jq .

echo ""
echo "========================================="
echo "Test 8: Accept rematch (should create new PLAYING match)"
echo "========================================="

sleep 1
REMATCH_ACC=$(curl -s -X POST http://localhost:5000/api/game/control \
  -H "Content-Type: application/json" \
  -d "{\"match_id\": $MATCH_ID, \"player_id\": 2, \"action\": \"REMATCH_ACCEPT\"}")

echo "$REMATCH_ACC" | jq .

NEW_MATCH_ID=$(echo "$REMATCH_ACC" | jq -r '.new_match_id')
echo ""
echo "New Match ID: $NEW_MATCH_ID"

echo ""
echo "========================================="
echo "Test 9: Check new match status (should be PLAYING immediately)"
echo "========================================="

sleep 1
STATUS=$(curl -s http://localhost:5000/api/game/status/$NEW_MATCH_ID)
echo "$STATUS" | jq .

echo ""
echo "========================================="
echo "Summary:"
echo "1. Initial match created with status WAITING"
echo "2. After player 2 joins -> status changed to PLAYING"
echo "3. After rematch accepted -> new match created with status PLAYING immediately"
echo "========================================="
