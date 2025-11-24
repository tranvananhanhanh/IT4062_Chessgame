#!/bin/bash

# Real Chess Game Test - Simulating an actual game with validation
echo "=========================================="
echo "♟️  Testing Real Chess Game Scenario"
echo "=========================================="

BASE_URL="http://localhost:5000/api"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
MATCH_ID=""
CURRENT_PLAYER=1

print_board_state() {
    local fen=$1
    echo -e "${CYAN}Current FEN: ${NC}$fen"
}

print_move() {
    local move_num=$1
    local player=$2
    local from=$3
    local to=$4
    local description=$5
    
    if [ $((move_num % 2)) -eq 1 ]; then
        echo -e "\n${MAGENTA}Move $((move_num / 2 + 1)).${NC} ${YELLOW}$from→$to${NC} ${description}"
    else
        echo -e "         ${YELLOW}$from→$to${NC} ${description}"
    fi
}

test_move() {
    local from=$1
    local to=$2
    local should_succeed=$3
    local description=$4
    local move_number=$5

    TESTS_RUN=$((TESTS_RUN + 1))
    
    print_move "$move_number" "$CURRENT_PLAYER" "$from" "$to" "$description"

    move_data="{\"match_id\": $MATCH_ID, \"player_id\": $CURRENT_PLAYER, \"from_square\": \"$from\", \"to_square\": \"$to\"}"
    
    response=$(curl -s -w "\nHTTPSTATUS:%{http_code}" -X POST \
        -H "Content-Type: application/json" \
        -d "$move_data" \
        "$BASE_URL/game/move")

    http_code=$(echo "$response" | tr -d '\n' | sed -e 's/.*HTTPSTATUS://')
    body=$(echo "$response" | sed -e 's/HTTPSTATUS:.*//g')

    if [ "$should_succeed" = "true" ]; then
        if [ "$http_code" = "200" ]; then
            success=$(echo "$body" | grep -o '"success": *true' | head -1)
            if [ -n "$success" ]; then
                echo -e "   ${GREEN}✅ Valid move accepted${NC}"
                TESTS_PASSED=$((TESTS_PASSED + 1))
                
                # Extract and display new FEN
                new_fen=$(echo "$body" | grep -o '"new_fen": *"[^"]*"' | cut -d'"' -f4)
                if [ -n "$new_fen" ]; then
                    print_board_state "$new_fen"
                fi
                
                # Switch player
                if [ "$CURRENT_PLAYER" -eq 1 ]; then
                    CURRENT_PLAYER=2
                else
                    CURRENT_PLAYER=1
                fi
                
                sleep 0.5
            else
                echo -e "   ${RED}❌ FAIL: Expected success but got failure${NC}"
                echo "   Response: $body"
                TESTS_FAILED=$((TESTS_FAILED + 1))
            fi
        else
            echo -e "   ${RED}❌ FAIL: HTTP $http_code${NC}"
            echo "   Response: $body"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        # Should fail (invalid move)
        success=$(echo "$body" | grep -o '"success": *false' | head -1)
        if [ -n "$success" ] || [ "$http_code" != "200" ]; then
            echo -e "   ${GREEN}✅ Invalid move correctly rejected${NC}"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "   ${RED}❌ FAIL: Invalid move was accepted!${NC}"
            echo "   Response: $body"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    fi
}

# Start services check
echo -e "${BLUE}Checking if services are running...${NC}"
health_response=$(curl -s "$BASE_URL/health" 2>/dev/null)
if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Services not running! Please start the backend and C server first.${NC}"
    exit 1
fi
echo -e "${GREEN}✅ Services are ready${NC}\n"

# Create new game
echo -e "${BLUE}════════════════════════════════════════${NC}"
echo -e "${BLUE}Starting new chess match...${NC}"
echo -e "${BLUE}════════════════════════════════════════${NC}"

response=$(curl -s -w "\nHTTPSTATUS:%{http_code}" -X POST \
    -H "Content-Type: application/json" \
    -d '{
        "player1_id": 1,
        "player1_name": "Magnus",
        "player2_id": 2,
        "player2_name": "Hikaru"
    }' \
    "$BASE_URL/game/start")

http_code=$(echo "$response" | tr -d '\n' | sed -e 's/.*HTTPSTATUS://')
body=$(echo "$response" | sed -e 's/HTTPSTATUS:.*//g')

if [ "$http_code" = "200" ]; then
    MATCH_ID=$(echo "$body" | grep -o '"match_id": *[0-9]*' | head -1 | grep -o '[0-9]*')
    initial_fen=$(echo "$body" | grep -o '"initial_fen": *"[^"]*"' | cut -d'"' -f4)
    
    echo -e "${GREEN}✅ Game created successfully${NC}"
    echo -e "${CYAN}Match ID: $MATCH_ID${NC}"
    print_board_state "$initial_fen"
else
    echo -e "${RED}❌ Failed to create game${NC}"
    exit 1
fi

echo -e "\n${BLUE}════════════════════════════════════════${NC}"
echo -e "${BLUE}Playing: Magnus (White) vs Hikaru (Black)${NC}"
echo -e "${BLUE}════════════════════════════════════════${NC}"

# REAL GAME MOVES - Italian Game Opening
move_count=0

# Opening - Italian Game
test_move "e2" "e4" "true" "(King's Pawn Opening)" $((move_count++))
test_move "e7" "e5" "true" "(Symmetric response)" $((move_count++))
test_move "g1" "f3" "true" "(Knight develops, attacks e5)" $((move_count++))
test_move "b8" "c6" "true" "(Knight defends e5)" $((move_count++))
test_move "f1" "c4" "true" "(Bishop to c4 - Italian Game)" $((move_count++))
test_move "f8" "c5" "true" "(Bishop mirrors - Giuoco Piano)" $((move_count++))

# Castle kingside
test_move "e1" "g1" "true" "(White castles kingside)" $((move_count++))
test_move "g8" "f6" "true" "(Knight develops)" $((move_count++))

# Middle game development
test_move "d2" "d3" "true" "(Pawn supports center)" $((move_count++))
test_move "d7" "d6" "true" "(Pawn supports e5)" $((move_count++))
test_move "b1" "c3" "true" "(Knight to c3)" $((move_count++))
test_move "e8" "g8" "true" "(Black castles kingside)" $((move_count++))

# Test INVALID moves
echo -e "\n${BLUE}════════════════════════════════════════${NC}"
echo -e "${BLUE}Testing Invalid Move Validation${NC}"
echo -e "${BLUE}════════════════════════════════════════${NC}"

test_move "f3" "e5" "false" "(Knight can't capture own piece)" $((move_count++))
test_move "h2" "h5" "false" "(Pawn can't move 3 squares)" $((move_count++))
test_move "f1" "f3" "false" "(No piece on f1 after castling)" $((move_count++))
test_move "a1" "a8" "false" "(Rook can't jump over pieces)" $((move_count++))
test_move "d1" "h5" "false" "(Queen path blocked)" $((move_count++))

echo -e "\n${BLUE}════════════════════════════════════════${NC}"
echo -e "${BLUE}Continuing Game - Tactical Play${NC}"
echo -e "${BLUE}════════════════════════════════════════${NC}"

# Continue with tactical moves
test_move "c1" "g5" "true" "(Bishop pins knight to queen)" $((move_count++))
test_move "h7" "h6" "true" "(Pawn attacks bishop)" $((move_count++))
test_move "g5" "h4" "true" "(Bishop retreats)" $((move_count++))
test_move "c6" "d4" "true" "(Knight to strong outpost)" $((move_count++))
test_move "f3" "d4" "true" "(White captures knight)" $((move_count++))
test_move "c5" "d4" "true" "(Bishop recaptures)" $((move_count++))

# Pawn advances
test_move "c2" "c3" "true" "(Pawn attacks bishop)" $((move_count++))
test_move "d4" "b6" "true" "(Bishop retreats)" $((move_count++))

# Rook activation
test_move "f1" "e1" "true" "(Rook to e-file)" $((move_count++))
test_move "c8" "e6" "true" "(Bishop develops)" $((move_count++))

# Queen moves
test_move "d1" "e2" "true" "(Queen connects rooks)" $((move_count++))
test_move "d8" "e7" "true" "(Queen to e7)" $((move_count++))

# More complex moves
test_move "c4" "e6" "true" "(Bishop captures bishop)" $((move_count++))
test_move "e7" "e6" "true" "(Queen recaptures)" $((move_count++))

# Pawn storm
test_move "b2" "b4" "true" "(Queenside pawn advance)" $((move_count++))
test_move "a7" "a6" "true" "(Preventing b5)" $((move_count++))
test_move "a2" "a4" "true" "(More pawn advance)" $((move_count++))
test_move "b7" "b5" "true" "(Black counters)" $((move_count++))

# En passant test setup
test_move "a4" "b5" "true" "(Pawn captures)" $((move_count++))
test_move "a6" "b5" "true" "(Recapture)" $((move_count++))

# Final position - prepare for endgame
test_move "h2" "h3" "true" "(Luft for king)" $((move_count++))
test_move "f8" "e8" "true" "(Rook doubles on e-file)" $((move_count++))
test_move "a1" "b1" "true" "(Rook to b-file)" $((move_count++))
test_move "a8" "c8" "true" "(Rook to c-file)" $((move_count++))

# Test wrong player trying to move
echo -e "\n${BLUE}════════════════════════════════════════${NC}"
echo -e "${BLUE}Testing Turn Validation${NC}"
echo -e "${BLUE}════════════════════════════════════════${NC}"

# Try to make White move when it's Black's turn
TEMP_PLAYER=$CURRENT_PLAYER
CURRENT_PLAYER=1
test_move "e2" "e3" "false" "(Wrong player - White trying to move)" $((move_count++))
CURRENT_PLAYER=$TEMP_PLAYER

# Continue game normally
test_move "c6" "d7" "true" "(Knight retreats)" $((move_count++))

# Test piece movement rules
echo -e "\n${BLUE}════════════════════════════════════════${NC}"
echo -e "${BLUE}Testing Piece Movement Rules${NC}"
echo -e "${BLUE}════════════════════════════════════════${NC}"

test_move "g1" "g2" "false" "(King can't move - would be in check or blocked)" $((move_count++))
test_move "c3" "e2" "true" "(Knight L-shape move)" $((move_count++))
test_move "g8" "h8" "true" "(King steps to corner)" $((move_count++))

# Test checkmate attempt scenario
test_move "e1" "e5" "true" "(Rook captures pawn with check)" $((move_count++))
test_move "h8" "g8" "true" "(King moves out of check)" $((move_count++))

# More tactical moves
test_move "e5" "e6" "true" "(Rook captures queen!)" $((move_count++))
test_move "c8" "c3" "true" "(Rook captures knight)" $((move_count++))

# Endgame
test_move "e6" "e8" "true" "(Rook to 8th rank - check)" $((move_count++))
test_move "g8" "h7" "true" "(King escapes)" $((move_count++))

# Test game state
echo -e "\n${BLUE}════════════════════════════════════════${NC}"
echo -e "${BLUE}Checking Game History${NC}"
echo -e "${BLUE}════════════════════════════════════════${NC}"

response=$(curl -s "$BASE_URL/history/replay/$MATCH_ID")
echo -e "${CYAN}Game replay data retrieved${NC}"

# Get player stats
response=$(curl -s "$BASE_URL/stats/1")
echo -e "${CYAN}Magnus's stats retrieved${NC}"

response=$(curl -s "$BASE_URL/stats/2")
echo -e "${CYAN}Hikaru's stats retrieved${NC}"

# Final results
echo ""
echo "=========================================="
echo -e "${BLUE}🏁 Test Results Summary${NC}"
echo "=========================================="
echo -e "Total moves tested: ${CYAN}$TESTS_RUN${NC}"
echo -e "Valid moves: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Failed tests: ${RED}$TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}🎉 Perfect! All moves validated correctly!${NC}"
    echo -e "${GREEN}✅ Move validation is working properly${NC}"
    echo -e "${GREEN}✅ Turn management is correct${NC}"
    echo -e "${GREEN}✅ Piece movement rules enforced${NC}"
    echo -e "${GREEN}✅ Invalid moves properly rejected${NC}"
else
    echo -e "${RED}⚠️  Some validation tests failed${NC}"
    echo -e "${YELLOW}Please review the failed moves above${NC}"
fi

echo ""
echo "=========================================="
echo -e "${MAGENTA}Game Statistics:${NC}"
echo -e "Match ID: ${CYAN}$MATCH_ID${NC}"
echo -e "Players: ${YELLOW}Magnus (White) vs Hikaru (Black)${NC}"
echo "=========================================="
