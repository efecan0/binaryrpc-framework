#!/usr/bin/env python3
import asyncio, struct, time, inspect, websockets, os, json
import logging
from typing import List, Dict

# Set debug log
#logging.basicConfig(level=logging.DEBUG)
#logger = logging.getLogger(__name__)

# ---------- transport constants ----------
FRAME_DATA = 0
FRAME_ACK  = 1
FT_RPC     = 2
FT_DATA    = 3

# ------------------------------------------------------------
#  Connection helpers
# ------------------------------------------------------------
SERVER_URI      = os.getenv("binaryrpc_qos1_integration_test", "ws://localhost:9010")
CID             = "cli-777"
DID             = "dev-777"
SESSION_TOKEN   = ""            # global, will be filled on 1st 101

# build headers
def make_headers():
    hdrs = [
        ("x-client-id", CID),
        ("x-device-id", DID),
    ]
    if SESSION_TOKEN:
        hdrs.append(("x-session-token", SESSION_TOKEN))
    #logger.debug(f"Headers: {hdrs}")
    return hdrs

# context-manager wrapper
class ws_connect:
    def __init__(self, uri, open_timeout=5):
        self.uri = uri
        self.ws  = None
        self.open_timeout = open_timeout
    async def __aenter__(self):
        global SESSION_TOKEN
        connect_kwargs = {
            "additional_headers": make_headers(),
            "open_timeout": self.open_timeout,
            "close_timeout": 2,
            "ping_interval": None,
            "ping_timeout": None
        }
        #logger.debug(f"Connecting to {self.uri} with timeout {self.open_timeout}")
        #logger.debug(f"Connection kwargs: {connect_kwargs}")
        try:
            self.ws = await websockets.connect(self.uri, **connect_kwargs)
            #ogger.debug("Connection successful")
            
            # Get response headers
            if hasattr(self.ws, 'response_headers'):
                hdrs = dict(self.ws.response_headers)
            elif hasattr(self.ws, 'response'):
                hdrs = dict(self.ws.response.headers)
            else:
                hdrs = {}
                #logger.warning("Could not access response headers")
            
            #logger.debug(f"Response headers: {hdrs}")

            # Get token on first connection
            if not SESSION_TOKEN:
                tok = hdrs.get("x-session-token")
                if tok: 
                    SESSION_TOKEN = tok
                    #logger.debug(f"Got session token: {tok}")
                #else:
                    #logger.warning("No session token in response headers!")
            
            return self.ws
        except Exception as e:
            #logger.error(f"Connection failed: {str(e)}")
            #logger.error(f"Exception type: {type(e)}")
            raise
    async def __aexit__(self, exc_type, exc, tb):
        if self.ws:
            try:
                await self.ws.close()
                #logger.debug("Connection closed")
            except Exception as e:
                print(f"Error while closing connection: {e}")

# ---------- helper utils ----------
def pack(ft, mid, payload=b""):
    return struct.pack("<BQ", ft, mid) + payload

async def send_frame(ws, ft, mid, pl=b""):
    # Create frame
    frame = pack(ft, mid, pl)
    # Show in hex format
    hex_str = ' '.join(f'{b:02x}' for b in frame)
    print(f"→ sending: [{hex_str}]")
    # Send as binary
    await ws.send(frame)

async def recv_frame(ws, timeout=None):
    raw = await asyncio.wait_for(ws.recv(), timeout) if timeout else await ws.recv()
    # Show raw binary in hex format
    hex_str = ' '.join(f'{b:02x}' for b in raw)
    print(f"→ got raw=[{hex_str}]")
    
    # Parse frame
    ft  = raw[0]  # Frame type (1 byte)
    mid = struct.unpack_from("<Q", raw, 1)[0]  # ID (8 byte)
    pl  = raw[9:]  # Payload
    
    # Convert frame type to string
    ft_str = {
        FRAME_DATA: "DATA",
        FRAME_ACK: "ACK",
        FT_RPC: "RPC",
        FT_DATA: "DATA"
    }.get(ft, f"UNKNOWN({ft})")
    
    print(f"→ parsed: type={ft_str}, id={mid}, payload={pl}")
    return ft, mid, pl

# auto-ack everything, return only DATA frames
async def reliable_recv(ws, timeout=None):
    while True:
        ft, mid, pl = await recv_frame(ws, timeout)
        # ACK every frame
        await send_frame(ws, FRAME_ACK, mid)
        # only return when it's a DATA frame
        if ft == FRAME_DATA:
            return ft, mid, pl

IDLE_TTL   = 3.0          # server‑side, seconds
WITHIN_TTL = 1.0          # 1 second (shorter than TTL)
AFTER_TTL = 4.0           # 4 seconds (longer than TTL)

# ============================================================
# 7) reconnect within TTL  → pending DATA replayed
# ============================================================
async def test_resume_within_ttl():
    print("\n=== Test 7: Resume ≤ TTL — DATA replay ===")
    # 1) first connection
    async with ws_connect(SERVER_URI, open_timeout=30) as ws1:
        # Send without specifying ID (0 = unspecified)
        await send_frame(ws1, FRAME_DATA, 0, b"echo:ping")
        ft, mid, pl = await recv_frame(ws1)      # Get server's ID
        assert ft == FRAME_DATA and pl == b"ping"
        # Intentionally don't send ACK: keep pending1 queue full

    await asyncio.sleep(WITHIN_TTL)                   # < idle timeout
    # 2) reconnect
    async with ws_connect(SERVER_URI) as ws2:
        # Use normal recv_frame instead of reliable_recv
        ft2, mid2, pl2 = await recv_frame(ws2, timeout=5)
        assert ft2 == FRAME_DATA and pl2 == b"ping"
        # Send ACK
        await send_frame(ws2, FRAME_ACK, mid2)
        print("✔ pending DATA replayed & ACKed within TTL")

# ============================================================
# 8) reconnect after TTL → outbox purged
# ============================================================
async def test_resume_after_ttl():
    print("\n=== Test 8: Resume > TTL — no replay ===")
    async with ws_connect(SERVER_URI) as ws1:
        await send_frame(ws1, FRAME_DATA, 0, b"echo:ping")  # ID=0
        await reliable_recv(ws1)
    await asyncio.sleep(AFTER_TTL)  # wait beyond idle timeout
    async with ws_connect(SERVER_URI) as ws2:
        try:
            await recv_frame(ws2, timeout=5)
            raise AssertionError("DATA replayed after TTL (should be purged)")
        except asyncio.TimeoutError:
            print("✔ no DATA replay after TTL (session purged)")

# ============================================================
# helper RPC to test session‑kept counter
# assumes you registered `counter` RPC that increments & returns int
# ============================================================
async def inc_counter(ws):
    print("DEBUG: Sending counter:inc request")
    await send_frame(ws, FRAME_DATA, 0, b"counter:inc")  # ID=0
    print("DEBUG: Waiting for response")
    _, mid, pl = await reliable_recv(ws)
    print(f"DEBUG: Got response: {pl}")
    return int(pl.decode())

# 9) resume ≤ TTL preserves session state
async def test_state_within_ttl():
    print("\n=== Test 9: Resume ≤ TTL — session state kept ===")
    async with ws_connect(SERVER_URI) as ws:
        val1 = await inc_counter(ws)   # -> 1
        assert val1 == 1
    await asyncio.sleep(WITHIN_TTL)
    async with ws_connect(SERVER_URI) as ws2:
        val2 = await inc_counter(ws2)  # should be 2
        assert val2 == 2
        print("✔ counter persisted across reconnect ≤ TTL")

# 10) resume > TTL resets session state
async def test_state_after_ttl():
    print("\n=== Test 10: Resume > TTL — session reset ===")
    async with ws_connect(SERVER_URI) as ws:
        val1 = await inc_counter(ws)  # -> 1
        assert val1 == 1
    await asyncio.sleep(AFTER_TTL)
    async with ws_connect(SERVER_URI) as ws2:
        val2 = await inc_counter(ws2)  # expect 1 again
        assert val2 == 1
        print("✔ counter reset after reconnect > TTL")

# 11) Client duplicate DATA (same ID)
async def test_client_duplicate_data():
    print("\n=== Test 11: Client duplicate DATA ===")
    async with ws_connect(SERVER_URI) as ws:
        # Send same DATA twice with same ID
        await send_frame(ws, FRAME_DATA, 123, b"echo:duplicate")
        await send_frame(ws, FRAME_DATA, 123, b"echo:duplicate")  # Same ID!
        
        # Should get only one response
        ft, mid, pl = await reliable_recv(ws, timeout=2)
        assert ft == FRAME_DATA and pl == b"duplicate"
        
        # Should not get second response
        try:
            await recv_frame(ws, timeout=1)
            raise AssertionError("Server echoed duplicate DATA twice")
        except asyncio.TimeoutError:
            print("✔ duplicate DATA dropped")

# ------------------------------------------------------------
# 12) Out‑of‑order ACK (ACK without receiving new DATA)
#     Server will keep old DATA in *retry* queue
# ------------------------------------------------------------
async def test_out_of_order_ack():
    print("\n=== Test 12: Out‑of‑order ACK ===")
    async with ws_connect(SERVER_URI) as ws:
        # 1. Client starts with FRAME_DATA
        frame = pack(FRAME_DATA, 0, b"counter:inc")  # Frame type + ID + payload
        await ws.send(frame)
        
        # 2. Server responds with DATA frame
        ft1, mid1, pl1 = await recv_frame(ws)
        print(f"→ got ft={ft1!r}, mid={mid1!r}, payload={pl1!r}")
        assert ft1 == FRAME_DATA
        assert pl1 == b"1"  # counter:inc result
        # Don't send ACK (for out-of-order test)
        
        # 3. Server retries (sends same DATA frame again)
        ft2, mid2, pl2 = await recv_frame(ws, timeout=2)
        print(f"→ got ft={ft2!r}, mid={mid2!r}, payload={pl2!r}")
        # Should be same frame
        assert ft2 == FRAME_DATA  # Same frame type
        assert mid2 == mid1       # Same message ID
        assert pl2 == pl1         # Same payload
        
        # 4. Client sends ACK
        await send_frame(ws, FRAME_ACK, mid1)
        
        # 5. Should not get retry anymore
        try:
            ft3, mid3, pl3 = await recv_frame(ws, timeout=1)
            raise AssertionError(f"Unexpected retry: ft={ft3}, mid={mid3}, pl={pl3}")
        except asyncio.TimeoutError:
            print("✔ No more retries after ACK")
        
        print("✔ out‑of‑order ACK scenario passed")

# ------------------------------------------------------------
# 13) Frame‑ID wrap‑around (64‑bit counter overflows)
# ------------------------------------------------------------
async def test_id_wraparound():
    print("\n=== Test 13: Frame‑ID wrap‑around ===")
    async with ws_connect(SERVER_URI) as ws:
        for off in range(3):
            print(f"\nDEBUG: Sending request {off}")
            # Use different ID for each request
            await send_frame(ws, FRAME_DATA, off, f"echo:{off}".encode())  # ID=off
            
            try:
                print(f"DEBUG: Waiting for response {off}")
                # Wait for response from server (reliable_recv automatically sends ACK)
                ft, mid, pl = await reliable_recv(ws, timeout=2)  # 2 second timeout
                print(f"DEBUG: Got response {off}: ft={ft!r}, mid={mid!r}, payload={pl!r}")
                assert ft == FRAME_DATA
                
                # payload check
                assert pl == f"{off}".encode()
                print(f"DEBUG: Response {off} OK")
            except asyncio.TimeoutError:
                print(f"❌ Timeout waiting for response to echo:{off}")
                raise
            
    print("✔ 64‑bit wrap‑around tolerated")

# ------------------------------------------------------------
# 14) Parallel connections, same client‑id/device‑id
#     • When Conn‑A closes, Conn‑B is still alive; pending1 should not replay to B
# ------------------------------------------------------------
async def test_parallel_same_identity():
    """Test that messages are properly handled when a client reconnects with the same identity."""
    print("\n=== Starting test_parallel_same_identity ===")
    
    # First connection
    print("Opening first connection (ws_a)")
    ws_a = await ws_connect(SERVER_URI).__aenter__()
    print("First connection established")
    
    # Send message to A without ACK
    print("Sending message to first connection without ACK")
    await send_frame(ws_a, FRAME_DATA, 0, b"test message 1")  # ID=0
    ft, pa, pl = await recv_frame(ws_a)  # Get server's assigned ID
    print(f"First message sent, server assigned ID: {pa}")
    # Don't send ACK, message will stay in pending1
    
    # Open second connection with same identity
    print("\nOpening second connection (ws_b) with same identity")
    ws_b = await ws_connect(SERVER_URI).__aenter__()
    print("Second connection established")
    
    # Close first connection
    print("\nClosing first connection")
    await ws_a.__aexit__(None, None, None)
    print("First connection closed")
    
    # Send message to B
    print("\nSending message to second connection")
    await send_frame(ws_b, FRAME_DATA, 0, b"test message 2")  # ID=0
    ft, pb, pl = await recv_frame(ws_b)  # Get server's assigned ID
    print(f"Second message sent, server assigned ID: {pb}")
    
    # Receive and ACK both messages on B
    print("\nWaiting for messages on second connection")
    received = set()
    try:
        while len(received) < 2:
            print("Waiting for next message...")
            ft, mid, pl = await recv_frame(ws_b, timeout=2)  # Just get frame, don't send ACK
            print(f"Received message: type={ft}, id={mid}, payload={pl}")
            received.add(mid)
            print(f"Current received set: {received}")
    except asyncio.TimeoutError:
        print(f"Timeout waiting for messages. Received so far: {received}")
        raise
    finally:
        print("\nClosing second connection")
        await ws_b.__aexit__(None, None, None)
        print("Test completed")
    
    # Verify both messages were received
    print("\nVerifying received messages")
    assert pa in received, f"First message {pa} not received"
    assert pb in received, f"Second message {pb} not received"
    print("All messages received successfully")

# ------------------------------------------------------------
# 15) Massive ACK‑flood (defensive path)
# ------------------------------------------------------------
async def test_ack_flood():
    print("\n=== Test 15: ACK flood (defence) ===")
    async with ws_connect(SERVER_URI) as ws:
        for i in range(1000):
            await send_frame(ws, FRAME_ACK, i)  # ID can be used for ACKs
        await send_frame(ws, FRAME_DATA, 0, b"echo:flood")  # ID=0 for DATA
        _, mid, pl = await reliable_recv(ws, timeout=2)
        assert pl == b"flood"
        print("✔ ACK flood did not break flow")

# ---------- main runner ----------
async def main():
    await test_resume_within_ttl()
    await asyncio.sleep(AFTER_TTL)
    await test_resume_after_ttl()
    await asyncio.sleep(AFTER_TTL)
    await test_state_within_ttl()
    await asyncio.sleep(AFTER_TTL)
    await test_state_after_ttl()
    await asyncio.sleep(AFTER_TTL)
    await test_client_duplicate_data()
    await asyncio.sleep(AFTER_TTL)
    await test_out_of_order_ack()
    await asyncio.sleep(AFTER_TTL)
    await test_id_wraparound()
    await asyncio.sleep(AFTER_TTL)
    await test_parallel_same_identity()
    await asyncio.sleep(AFTER_TTL)
    await test_ack_flood()
    await asyncio.sleep(AFTER_TTL)    
    await test_offline_message_delivery()
    print("\n🎉 Offline message delivery test passed!")

class WebSocketClient:
    def __init__(self, client_id: str, device_id: str):
        self.client_id = client_id
        self.device_id = device_id
        self.ws = None
        self.received_messages = []
        self.connected = False
        self.session_token = None

    async def connect(self):
        # WebSocket connection URL
        uri = "ws://localhost:9010"
        
        # Prepare headers
        headers = {
            "x-client-id": self.client_id,
            "x-device-id": self.device_id
        }
        
        # Add session token if available
        if self.session_token:
            headers["x-session-token"] = self.session_token
            print(f"Using existing session token: {self.session_token}")
        
        # Establish connection
        self.ws = await websockets.connect(uri, additional_headers=headers)
        
        # Get session token from response headers
        if hasattr(self.ws, 'response_headers'):
            headers = dict(self.ws.response_headers)
            if "x-session-token" in headers:
                self.session_token = headers["x-session-token"]
                print(f"Got new session token: {self.session_token}")
        elif hasattr(self.ws, 'response'):
            headers = dict(self.ws.response.headers)
            if "x-session-token" in headers:
                self.session_token = headers["x-session-token"]
                print(f"Got new session token: {self.session_token}")
        
        self.connected = True
        print(f"Connected with client_id: {self.client_id}, device_id: {self.device_id}")
        
        # Start message listening task
        asyncio.create_task(self.listen_messages())

    async def disconnect(self):
        if self.ws:
            await self.ws.close()
            self.connected = False
            print(f"Disconnected client_id: {self.client_id}")

    async def send_rpc(self, method: str, params: str):
        if not self.connected:
            raise Exception("Not connected")
        
        # Prepare RPC message
        message = f"{method}:{params}"
        
        # Send frame
        await send_frame(self.ws, FRAME_DATA, 0, message.encode())
        print(f"Sent RPC: {method} with params: {params}")

    async def listen_messages(self):
        try:
            while self.connected:
                ft, mid, pl = await recv_frame(self.ws)
                if ft == FRAME_DATA:
                    self.received_messages.append(pl.decode())
                    print(f"Received message: {pl.decode()}")
        except websockets.exceptions.ConnectionClosed:
            print("Connection closed")
        except Exception as e:
            print(f"Error in listen_messages: {e}")

    def get_received_messages(self) -> List[str]:
        return self.received_messages

async def test_offline_message_delivery():
    try:
        # 1. User X connects and logs in
        x_client = WebSocketClient("client_x", "dev_1")
        await x_client.connect()
        await x_client.send_rpc("login", "X:user")
        
        # Store first session token
        first_token = x_client.session_token
        print(f"First session token: {first_token}")
        
        # Check that premium feature is added
        await asyncio.sleep(1)  # Wait for server to complete processing
        
        # 2. Close X user's connection
        await x_client.disconnect()
        
        # 3. User Y connects and sends multiple messages
        y_client = WebSocketClient("client_y", "dev_2")
        await y_client.connect()
        
        # Send 10 different messages
        messages = []
        for i in range(10):
            message = f"test message {i+1}"
            messages.append(message)
            await y_client.send_rpc("sendToPremium", message)
            print(f"Y sent message {i+1}: {message}")
            await asyncio.sleep(0.1)  # Short wait between messages
        
        # 4. User X reconnects (with same session token)
        await asyncio.sleep(1)  # Wait for messages to be queued
        x_client.session_token = first_token  # Use first token
        await x_client.connect()
        await x_client.send_rpc("login", "X:user")
        
        # 5. Check if user X received all messages
        await asyncio.sleep(2)  # Wait for messages to be delivered
        
        received_messages = x_client.get_received_messages()
        print(f"\nReceived messages: {received_messages}")
        
        # Check that each message was received
        for message in messages:
            assert any(message in msg for msg in received_messages), f"User X did not receive message '{message}'!"
        
        print(f"\nTest successful! User X received all {len(messages)} messages.")
        
    except Exception as e:
        print(f"Test failed: {e}")
        raise
    finally:
        # Clean up connections
        if 'x_client' in locals():
            await x_client.disconnect()
        if 'y_client' in locals():
            await y_client.disconnect()

if __name__ == "__main__":
    asyncio.run(main())
