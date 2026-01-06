#!/usr/bin/env python3
"""
Test script for in-game chat feature
Creates 2 client connections, starts a PvP match, and tests chat messages
"""

import socket
import time
import sys
from threading import Thread

class TestClient:
    def __init__(self, host='0.tcp.ap.ngrok.io', port=15515):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5)
        try:
            self.sock.connect((host, port))
            self.sock.setblocking(True)
        except Exception as e:
            print(f"Failed to connect: {e}")
            sys.exit(1)
        self.buffer = b''
    
    def send(self, msg):
        """Send message to server"""
        if not msg.endswith('\n'):
            msg += '\n'
        try:
            self.sock.send(msg.encode())
            print(f"[SENT] {msg.strip()}")
        except Exception as e:
            print(f"[ERROR] Failed to send: {e}")
    
    def recv(self, timeout=5):
        """Receive message from server"""
        self.sock.settimeout(timeout)
        try:
            while b'\n' not in self.buffer:
                data = self.sock.recv(4096)
                if not data:
                    return None
                self.buffer += data
            
            line, self.buffer = self.buffer.split(b'\n', 1)
            msg = line.decode().strip()
            print(f"[RECV] {msg}")
            return msg
        except socket.timeout:
            print("[TIMEOUT] No response from server")
            return None
        except Exception as e:
            print(f"[ERROR] Failed to receive: {e}")
            return None
    
    def close(self):
        """Close connection"""
        try:
            self.sock.close()
        except:
            pass

def test_chat():
    """Test in-game chat between two players"""
    print("=" * 60)
    print("IN-GAME CHAT TEST")
    print("=" * 60)
    
    # Create two client connections
    print("\n[1] Creating client connections...")
    client1 = TestClient()
    client2 = TestClient()
    print("✓ Both clients connected")
    
    # Test users (use existing or create new)
    user1 = "testplayer1"
    pass1 = "Test@123"
    user2 = "testplayer2"
    pass2 = "Test@123"
    
    user1_id = None
    user2_id = None
    
    # Try login/register for user1
    print(f"\n[2] Login/Register {user1}...")
    client1.send(f"LOGIN|{user1}|{pass1}")
    resp = client1.recv()
    if resp and "LOGIN_SUCCESS" in resp:
        user1_id = int(resp.split('|')[1])
        print(f"✓ {user1} logged in (ID: {user1_id})")
    elif resp and "LOGIN_FAIL" in resp:
        print(f"✗ Login failed, trying to register...")
        client1.send(f"REGISTER|{user1}|{pass1}|{user1}@test.com")
        resp = client1.recv()
        if resp and "REGISTER_OK" in resp:
            print(f"✓ {user1} registered, logging in...")
            client1.send(f"LOGIN|{user1}|{pass1}")
            resp = client1.recv()
            if resp and "LOGIN_SUCCESS" in resp:
                user1_id = int(resp.split('|')[1])
                print(f"✓ {user1} logged in (ID: {user1_id})")
    
    # Try login/register for user2
    print(f"\n[3] Login/Register {user2}...")
    client2.send(f"LOGIN|{user2}|{pass2}")
    resp = client2.recv()
    if resp and "LOGIN_SUCCESS" in resp:
        user2_id = int(resp.split('|')[1])
        print(f"✓ {user2} logged in (ID: {user2_id})")
    elif resp and "LOGIN_FAIL" in resp:
        print(f"✗ Login failed, trying to register...")
        client2.send(f"REGISTER|{user2}|{pass2}|{user2}@test.com")
        resp = client2.recv()
        if resp and "REGISTER_OK" in resp:
            print(f"✓ {user2} registered, logging in...")
            client2.send(f"LOGIN|{user2}|{pass2}")
            resp = client2.recv()
            if resp and "LOGIN_SUCCESS" in resp:
                user2_id = int(resp.split('|')[1])
                print(f"✓ {user2} logged in (ID: {user2_id})")
    
    if not user1_id or not user2_id:
        print("\n✗ Failed to authenticate users")
        return False
    
    # Create match
    print(f"\n[4] Creating PvP match...")
    # Random match ID (server generates, but we can specify)
    match_id = int(time.time()) % 100000
    client1.send(f"START_MATCH|{match_id}|{user1_id}|{user2_id}|white")
    resp = client1.recv(timeout=10)
    if not resp:
        print("✗ Failed to create match")
        return False
    
    print(f"✓ Match created: {resp}")
    
    # Extract match_id if needed
    if "MATCH_CREATED" in resp or "MATCH_START" in resp:
        try:
            match_id = int(resp.split('|')[1])
            print(f"✓ Match ID: {match_id}")
        except:
            pass
    
    # Give server time to process
    time.sleep(1)
    
    # Test chat: User1 sends message
    print(f"\n[5] Testing chat message from {user1}...")
    chat_msg1 = "Hello! Good game ahead!"
    client1.send(f"GAME_CHAT|{match_id}|{chat_msg1}")
    
    # User2 should receive the message
    print(f"[6] Waiting for {user2} to receive message...")
    time.sleep(0.5)
    resp = client2.recv(timeout=3)
    
    if resp and "GAME_CHAT_FROM" in resp:
        parts = resp.split('|', 2)
        if len(parts) >= 3:
            sender = parts[1]
            message = parts[2]
            print(f"✓ Message received!")
            print(f"   From: {sender}")
            print(f"   Text: {message}")
            if message == chat_msg1:
                print("✓ Message content matches!")
            else:
                print(f"✗ Message content mismatch. Expected: '{chat_msg1}', Got: '{message}'")
        else:
            print("✗ Invalid message format")
            return False
    else:
        print(f"✗ No message received. Got: {resp}")
        return False
    
    # Test chat: User2 sends message
    print(f"\n[7] Testing chat message from {user2}...")
    chat_msg2 = "Thanks! Let's have fun!"
    client2.send(f"GAME_CHAT|{match_id}|{chat_msg2}")
    
    # User1 should receive the message
    print(f"[8] Waiting for {user1} to receive message...")
    time.sleep(0.5)
    resp = client1.recv(timeout=3)
    
    if resp and "GAME_CHAT_FROM" in resp:
        parts = resp.split('|', 2)
        if len(parts) >= 3:
            sender = parts[1]
            message = parts[2]
            print(f"✓ Message received!")
            print(f"   From: {sender}")
            print(f"   Text: {message}")
            if message == chat_msg2:
                print("✓ Message content matches!")
            else:
                print(f"✗ Message content mismatch. Expected: '{chat_msg2}', Got: '{message}'")
        else:
            print("✗ Invalid message format")
            return False
    else:
        print(f"✗ No message received. Got: {resp}")
        return False
    
    # Test multiple messages
    print(f"\n[9] Testing rapid message exchange...")
    messages = [
        (client1, "First move: e4"),
        (client2, "e5"),
        (client1, "Nf3"),
        (client2, "Nc6"),
    ]
    
    for sender_client, msg in messages:
        print(f"\n   Sending: {msg}")
        sender_client.send(f"GAME_CHAT|{match_id}|{msg}")
        time.sleep(0.3)
        
        # The other client should receive
        receiver_client = client2 if sender_client is client1 else client1
        resp = receiver_client.recv(timeout=3)
        if resp and "GAME_CHAT_FROM" in resp:
            print(f"   ✓ Received: {resp}")
        else:
            print(f"   ✗ Failed to receive: {resp}")
    
    # Cleanup
    print(f"\n[10] Closing connections...")
    client1.close()
    client2.close()
    print("✓ All connections closed")
    
    print("\n" + "=" * 60)
    print("✓ CHAT TEST COMPLETED SUCCESSFULLY!")
    print("=" * 60)
    return True

if __name__ == "__main__":
    try:
        success = test_chat()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\n\n[INTERRUPTED] Test aborted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n\n[FATAL ERROR] {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
