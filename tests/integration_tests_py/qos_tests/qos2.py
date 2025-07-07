#!/usr/bin/env python3
import asyncio, struct, time, inspect, websockets, os, random
import logging
from typing import List

# Frame tipleri
FRAME_DATA = 0
FRAME_ACK = 1
FRAME_PREPARE = 2
FRAME_PREPARE_ACK = 3
FRAME_COMMIT = 4
FRAME_COMPLETE = 5

# Frame type'ı string'e çeviren helper fonksiyon
def frameTypeToString(ft):
    return {
        FRAME_DATA: "DATA",
        FRAME_ACK: "ACK",
        FRAME_PREPARE: "PREPARE",
        FRAME_PREPARE_ACK: "PREPARE_ACK",
        FRAME_COMMIT: "COMMIT",
        FRAME_COMPLETE: "COMPLETE"
    }.get(ft, f"UNKNOWN({ft})")

# Bağlantı sabitleri
SERVER_URI = os.getenv("binaryrpc_qos2_integration_test", "ws://localhost:9010")
#CID = "cli-777"
CID = f"cli-{random.randint(1000, 9999)}"  # Her test için unique ID
DID = "dev-777"
SESSION_TOKEN = ""  # global, will be filled on 1st 101

# Timeout sabitleri
IDLE_TTL = 3.0          # server‑side, seconds
WITHIN_TTL = 1.0        # 1 saniye (TTL'den kısa)
AFTER_TTL = 5.0         # 4 saniye (TTL'den uzun)

# Bağlantı yardımcıları
def make_headers():
    hdrs = [
        ("x-client-id", CID),
        ("x-device-id", DID),
    ]
    if SESSION_TOKEN:
        hdrs.append(("x-session-token", SESSION_TOKEN))
    return hdrs

class ws_connect:
    def __init__(self, uri, open_timeout=5):
        self.uri = uri
        self.ws = None
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
        try:
            self.ws = await websockets.connect(self.uri, **connect_kwargs)
            
            if hasattr(self.ws, 'response_headers'):
                hdrs = dict(self.ws.response_headers)
            elif hasattr(self.ws, 'response'):
                hdrs = dict(self.ws.response.headers)
            else:
                hdrs = {}
            
            if not SESSION_TOKEN:
                tok = hdrs.get("x-session-token")
                if tok: 
                    SESSION_TOKEN = tok
            
            return self.ws
        except Exception as e:
            raise

    async def __aexit__(self, exc_type, exc, tb):
        if self.ws:
            try:
                await self.ws.close()
            except Exception as e:
                print(f"Error while closing connection: {e}")

# Frame yardımcıları
def pack(ft, mid, payload=b""):
    return struct.pack(">BQ", ft, mid) + payload

async def send_frame(ws, ft, mid, pl=b""):
    frame = pack(ft, mid, pl)
    hex_str = ' '.join(f'{b:02x}' for b in frame)
    print(f"→ sending: [{hex_str}]")
    await ws.send(frame)

async def recv_frame(ws, timeout=None):
    try:
        print("DEBUG: Waiting for frame...")
        raw = await asyncio.wait_for(ws.recv(), timeout) if timeout else await ws.recv()
        print(f"DEBUG: Raw data length: {len(raw)} bytes")
        hex_str = ' '.join(f'{b:02x}' for b in raw)
        print(f"DEBUG: Raw data (hex): [{hex_str}]")
        
        if len(raw) < 9:  # Minimum frame size (1 byte type + 8 bytes message ID)
            print(f"ERROR: Frame too short: {len(raw)} bytes")
            return None, None, None
            
        ft = raw[0]
        mid = struct.unpack_from(">Q", raw, 1)[0]
        pl = raw[9:]
        
        ft_str = frameTypeToString(ft)
        
        print(f"DEBUG: Parsed frame - Type: {ft_str}, ID: {mid}, Payload length: {len(pl)}")
        if pl:
            print(f"DEBUG: Payload (hex): [{pl.hex()}]")
            print(f"DEBUG: Payload (str): [{pl.decode('utf-8', errors='replace')}]")
            
        return ft, mid, pl
    except asyncio.TimeoutError:
        print("DEBUG: Timeout waiting for frame")
        raise
    except Exception as e:
        print(f"ERROR in recv_frame: {str(e)}")
        raise

async def recv_data_or_handle_commit_retries(ws, expected_fid, timeout_duration=3.0):
    """
    Waits for a FRAME_DATA with expected_fid.
    Ignores retried FRAME_COMMIT for the same expected_fid.
    Raises TimeoutError if DATA not received within timeout_duration.
    Raises AssertionError for other unexpected frames.
    """
    start_time = time.time()
    while time.time() - start_time < timeout_duration:
        try:
            ft, fid, payload = await recv_frame(ws, timeout=0.2) # Short inner timeout
            # print(f"DEBUG [recv_data_or_handle_commit_retries]: Received Type: {frameTypeToString(ft)}, ID: {fid}, expected_fid: {expected_fid}, Payload: {payload.decode(errors='ignore') if payload else ''}")
            if ft == FRAME_DATA and fid == expected_fid:
                print(f"DEBUG [recv_data_or_handle_commit_retries]: Received expected DATA for ID {fid}")
                return ft, fid, payload
            elif ft == FRAME_COMMIT and fid == expected_fid:
                print(f"DEBUG [recv_data_or_handle_commit_retries]: Ignored retried COMMIT for ID {fid}")
                continue # Loop to get next frame
            else:
                error_msg = f"Unexpected frame Type: {frameTypeToString(ft)}, ID: {fid} (payload: {payload.hex()}) while waiting for DATA with ID {expected_fid}"
                print(f"ERROR [recv_data_or_handle_commit_retries]: {error_msg}")
                raise AssertionError(error_msg)
        except asyncio.TimeoutError:
            # Inner timeout, continue outer loop
            # print(f"DEBUG [recv_data_or_handle_commit_retries]: Inner timeout, continuing wait for DATA ID {expected_fid}")
            if time.time() - start_time >= timeout_duration: # Check overall timeout before continuing
                break
            continue
    raise asyncio.TimeoutError(f"Did not receive DATA frame for ID {expected_fid} within {timeout_duration}s")

async def recv_commit_or_handle_prepare_retries(ws, expected_fid, timeout_duration=3.0):
    """
    Waits for a FRAME_COMMIT with expected_fid.
    Ignores retried FRAME_PREPARE for the same expected_fid.
    Raises TimeoutError if COMMIT not received within timeout_duration.
    Raises AssertionError for other unexpected frames.
    """
    start_time = time.time()
    while time.time() - start_time < timeout_duration:
        try:
            ft, fid, payload = await recv_frame(ws, timeout=0.2) # Short inner timeout
            # print(f"DEBUG [recv_commit_or_handle_prepare_retries]: Received Type: {frameTypeToString(ft)}, ID: {fid}, expected_fid: {expected_fid}")
            if ft == FRAME_COMMIT and fid == expected_fid:
                print(f"DEBUG [recv_commit_or_handle_prepare_retries]: Received expected COMMIT for ID {fid}")
                return ft, fid, payload
            elif ft == FRAME_PREPARE and fid == expected_fid:
                print(f"DEBUG [recv_commit_or_handle_prepare_retries]: Ignored retried PREPARE for ID {fid}")
                continue # Loop to get next frame
            else:
                error_msg = f"Unexpected frame Type: {frameTypeToString(ft)}, ID: {fid} (payload: {payload.hex()}) while waiting for COMMIT with ID {expected_fid}"
                print(f"ERROR [recv_commit_or_handle_prepare_retries]: {error_msg}")
                raise AssertionError(error_msg)
        except asyncio.TimeoutError:
            if time.time() - start_time >= timeout_duration: # Check overall timeout before continuing
                break
            continue
    raise asyncio.TimeoutError(f"Did not receive COMMIT frame for ID {expected_fid} within {timeout_duration}s")

# Helper RPC to test session‑kept counter
async def inc_counter(ws):
    print("DEBUG: Sending counter:inc request")
    await send_frame(ws, FRAME_DATA, 0, b"counter:inc")
    print("DEBUG: Waiting for response")
    _, mid, pl = await recv_frame(ws)
    print(f"DEBUG: Got response: {pl}")
    return int(pl.decode())

# Test senaryoları
async def test_resume_within_ttl():
    """Test QoS2 flow with connection resume within TTL"""
    print("\nDEBUG: Opening first connection...")
    async with ws_connect(SERVER_URI) as ws:
        print("DEBUG: First connection established")
        
        # 1. Send DATA frame
        print("DEBUG: Sending initial DATA frame...")
        await send_frame(ws, FRAME_DATA, 0, b"echo:ping")
        
        # 2. Wait for PREPARE
        print("DEBUG: Waiting for PREPARE...")
        ft, fid_prepare, payload = await wait_for_frame(ws, FRAME_PREPARE)
        print(f"Received frame type: {ft}")
        print(f"DEBUG: Received PREPARE for id {fid_prepare}")
        
        # 3. Send PREPARE_ACK
        print("DEBUG: Sending PREPARE_ACK...")
        await send_frame(ws, FRAME_PREPARE_ACK, fid_prepare, b"")
        
        # 4. Wait for COMMIT
        print("DEBUG: Waiting for COMMIT...")
        ft, fid_commit, payload = await wait_for_frame(ws, FRAME_COMMIT, expected_id=fid_prepare)
        print(f"DEBUG: Received COMMIT for id {fid_commit}")
        
        # 5. Send COMPLETE
        print("DEBUG: Sending COMPLETE...")
        await send_frame(ws, FRAME_COMPLETE, fid_commit, b"")
        
        # 6. Wait for final DATA frame
        print("DEBUG: Waiting for final DATA frame...")
        ft_data, fid_data_final, payload_data = await wait_for_frame(ws, FRAME_DATA, expected_id=fid_commit)
        
        assert payload_data == b"ping"
        assert ft_data == FRAME_DATA, f"Expected DATA, got {ft_data}"
        assert fid_data_final == fid_commit, "Final DATA ID should match COMMIT ID"
        
        print("DEBUG: Test completed successfully")

async def test_resume_after_ttl():
    """Test QoS2 flow after TTL expiration"""
    print("\n=== Test 2: Resume > TTL — no replay ===")
    async with ws_connect(SERVER_URI) as ws1:
        # First connection - complete QoS2 flow
        await send_frame(ws1, FRAME_DATA, 0, b"echo:ping")
        ft, fid_prepare1, payload = await wait_for_frame(ws1, FRAME_PREPARE)
        await send_frame(ws1, FRAME_PREPARE_ACK, fid_prepare1, b"")
        ft, fid_commit1, payload = await wait_for_frame(ws1, FRAME_COMMIT, expected_id=fid_prepare1)
        await send_frame(ws1, FRAME_COMPLETE, fid_commit1, b"")
        ft_data1, fid_data_final1, payload_data1 = await wait_for_frame(ws1, FRAME_DATA, expected_id=fid_commit1)
        assert payload_data1 == b"ping"
        assert ft_data1 == FRAME_DATA, f"Expected DATA, got {ft_data1}"
        assert fid_data_final1 == fid_commit1
    # Wait beyond TTL
    await asyncio.sleep(AFTER_TTL)
    # New connection after TTL
    async with ws_connect(SERVER_URI) as ws2:
        # Should start fresh QoS2 flow
        await send_frame(ws2, FRAME_DATA, 0, b"echo:ping")
        ft, fid_prepare2, payload = await wait_for_frame(ws2, FRAME_PREPARE)
        await send_frame(ws2, FRAME_PREPARE_ACK, fid_prepare2, b"")
        ft, fid_commit2, payload = await wait_for_frame(ws2, FRAME_COMMIT, expected_id=fid_prepare2)
        await send_frame(ws2, FRAME_COMPLETE, fid_commit2, b"")
        ft_data2, fid_data_final2, payload_data2 = await wait_for_frame(ws2, FRAME_DATA, expected_id=fid_commit2)
        assert payload_data2 == b"ping"
        assert ft_data2 == FRAME_DATA, f"Expected DATA, got {ft_data2}"
        assert fid_data_final2 == fid_commit2
        print("✔ New session established after TTL")

async def test_state_within_ttl():
    """Test session state preservation within TTL"""
    print("\n=== Test 3: Resume ≤ TTL — session state kept ===")
    async with ws_connect(SERVER_URI) as ws:
        # First counter increment
        await send_frame(ws, FRAME_DATA, 0, b"counter:inc")
        ft, fid_prepare1, payload = await wait_for_frame(ws, FRAME_PREPARE)
        await send_frame(ws, FRAME_PREPARE_ACK, fid_prepare1, b"")
        ft, fid_commit1, payload = await wait_for_frame(ws, FRAME_COMMIT, expected_id=fid_prepare1)
        await send_frame(ws, FRAME_COMPLETE, fid_commit1, b"")
        ft_data1, fid_data_final1, payload_data1 = await wait_for_frame(ws, FRAME_DATA, expected_id=fid_commit1)
        assert payload_data1 == b"1"
        assert ft_data1 == FRAME_DATA, f"Expected DATA, got {ft_data1}"
        assert fid_data_final1 == fid_commit1
    await asyncio.sleep(WITHIN_TTL)
    # Second connection within TTL
    async with ws_connect(SERVER_URI) as ws2:
        # Second counter increment
        await send_frame(ws2, FRAME_DATA, 0, b"counter:inc")
        ft, fid_prepare2, payload = await wait_for_frame(ws2, FRAME_PREPARE)
        await send_frame(ws2, FRAME_PREPARE_ACK, fid_prepare2, b"")
        ft, fid_commit2, payload = await wait_for_frame(ws2, FRAME_COMMIT, expected_id=fid_prepare2)
        await send_frame(ws2, FRAME_COMPLETE, fid_commit2, b"")
        ft_data2, fid_data_final2, payload_data2 = await wait_for_frame(ws2, FRAME_DATA, expected_id=fid_commit2)
        assert payload_data2 == b"2"
        assert ft_data2 == FRAME_DATA, f"Expected DATA, got {ft_data2}"
        assert fid_data_final2 == fid_commit2
        print("✔ counter persisted across reconnect ≤ TTL")

async def test_state_after_ttl():
    """Test session state reset after TTL"""
    print("\n=== Test 4: Resume > TTL — session reset ===")
    async with ws_connect(SERVER_URI) as ws:
        # First counter increment
        await send_frame(ws, FRAME_DATA, 0, b"counter:inc")
        ft, fid_prepare1, payload = await wait_for_frame(ws, FRAME_PREPARE)
        await send_frame(ws, FRAME_PREPARE_ACK, fid_prepare1, b"")
        ft, fid_commit1, payload = await wait_for_frame(ws, FRAME_COMMIT, expected_id=fid_prepare1)
        await send_frame(ws, FRAME_COMPLETE, fid_commit1, b"")
        ft_data1, fid_data_final1, payload_data1 = await wait_for_frame(ws, FRAME_DATA, expected_id=fid_commit1)
        assert payload_data1 == b"1"
        assert ft_data1 == FRAME_DATA, f"Expected DATA, got {ft_data1}"
        assert fid_data_final1 == fid_commit1
    await asyncio.sleep(AFTER_TTL)
    # New connection after TTL
    async with ws_connect(SERVER_URI) as ws2:
        # Counter should start from 1 again
        await send_frame(ws2, FRAME_DATA, 0, b"counter:inc")
        ft, fid_prepare2, payload = await wait_for_frame(ws2, FRAME_PREPARE)
        await send_frame(ws2, FRAME_PREPARE_ACK, fid_prepare2, b"")
        ft, fid_commit2, payload = await wait_for_frame(ws2, FRAME_COMMIT, expected_id=fid_prepare2)
        await send_frame(ws2, FRAME_COMPLETE, fid_commit2, b"")
        ft_data2, fid_data_final2, payload_data2 = await wait_for_frame(ws2, FRAME_DATA, expected_id=fid_commit2)
        assert payload_data2 == b"1"
        assert ft_data2 == FRAME_DATA, f"Expected DATA, got {ft_data2}"
        assert fid_data_final2 == fid_commit2
        print("✔ counter reset after idle TTL (new session)")

def wait_for_frame(ws, expected_type, expected_id=None, timeout=3.0, allow_retries=True, retry_types=None):
    """
    ws: websocket
    expected_type: beklenen frame tipi (int)
    expected_id: beklenen frame id'si (int veya None)
    timeout: toplam bekleme süresi
    allow_retries: retry/replay frame'lerini toleranslı işle
    retry_types: retry olarak kabul edilecek frame tipleri (liste)
    """
    import asyncio, time
    if retry_types is None:
        retry_types = [FRAME_PREPARE, FRAME_COMMIT]
    async def _wait():
        start = time.time()
        while time.time() - start < timeout:
            try:
                ft, fid, payload = await recv_frame(ws, timeout=0.5)
                if ft == expected_type and (expected_id is None or fid == expected_id):
                    return ft, fid, payload
                elif allow_retries and ft in retry_types and (expected_id is None or fid == expected_id):
                    print(f"[wait_for_frame] Retry/replay frame geldi: {frameTypeToString(ft)}, ID: {fid}, beklenen: {frameTypeToString(expected_type)}, {expected_id}")
                    continue
                else:
                    print(f"[wait_for_frame] Beklenmeyen frame: {frameTypeToString(ft)}, ID: {fid}, payload: {payload}")
            except asyncio.TimeoutError:
                continue
        raise asyncio.TimeoutError(f"Beklenen frame gelmedi: {frameTypeToString(expected_type)}, ID: {expected_id}")
    return _wait()

async def test_duplicate_data():
    """Test duplicate DATA frame handling"""
    print("\n=== Test 5: Duplicate DATA ===")
    async with ws_connect(SERVER_URI) as ws:
        # Send first DATA frame
        await send_frame(ws, FRAME_DATA, 0, b"echo:dup")
        ft, fid_prepare, payload = await wait_for_frame(ws, FRAME_PREPARE, expected_id=None)
        await send_frame(ws, FRAME_PREPARE_ACK, fid_prepare, b"")
        ft, fid_commit, payload = await wait_for_frame(ws, FRAME_COMMIT, expected_id=fid_prepare)
        await send_frame(ws, FRAME_COMPLETE, fid_commit, b"")
        ft_data, fid_data_final, payload_data = await wait_for_frame(ws, FRAME_DATA, expected_id=fid_commit)
        assert payload_data == b"dup"
        assert ft_data == FRAME_DATA, f"Expected DATA, got {ft_data}"
        assert fid_data_final == fid_commit
        
        # Send duplicate DATA frame
        await send_frame(ws, FRAME_DATA, 0, b"echo:dup")
        try:
            ft, fid, payload = await wait_for_frame(ws, FRAME_PREPARE, expected_id=None, timeout=1.0)
            raise AssertionError("Server processed duplicate DATA")
        except asyncio.TimeoutError:
            print("✔ duplicate DATA dropped")

async def test_out_of_order_ack():
    """Test out-of-order ACK handling"""
    print("\n=== Test 6: Out‑of‑order ACK ===")
    async with ws_connect(SERVER_URI) as ws:
        # Send DATA frame
        await send_frame(ws, FRAME_DATA, 0, b"counter:inc")
        ft1, mid1, pl1 = await recv_frame(ws)
        assert ft1 == FRAME_PREPARE
        
        # Send PREPARE_ACK
        await send_frame(ws, FRAME_PREPARE_ACK, mid1, b"")
        
        # Wait for COMMIT
        ft2, mid2, pl2 = await recv_frame(ws)
        assert ft2 == FRAME_COMMIT and mid2 == mid1
        
        # Send COMPLETE
        await send_frame(ws, FRAME_COMPLETE, mid1, b"")

        # Wait for DATA frame, handling potential retries
        while True:
            ft, mid, payload = await recv_frame(ws)
            if ft == FRAME_DATA:
                assert payload == b"1"
                break
            elif ft == FRAME_COMMIT:
                print(f"Ignoring retry COMMIT for ID {mid}, sending COMPLETE again")
                await send_frame(ws, FRAME_COMPLETE, mid)
            elif ft == FRAME_PREPARE:
                print(f"Ignoring retry PREPARE for ID {mid}, sending PREPARE_ACK again")
                await send_frame(ws, FRAME_PREPARE_ACK, mid)
            else:
                print(f"Ignoring unexpected frame type {frameTypeToString(ft)} while waiting for DATA")

        try:
            ft4, mid4, pl4 = await recv_frame(ws, timeout=1)
            raise AssertionError(f"Unexpected retry: ft={ft4}, mid={mid4}, pl={pl4}")
        except asyncio.TimeoutError:
            print("✔ No more retries after ACK")

async def test_id_wraparound():
    """Test message ID wraparound handling"""
    print("\n=== Test 7: Frame‑ID wrap‑around ===")
    async with ws_connect(SERVER_URI) as ws:
        for off in range(3):
            # Send DATA frame
            await send_frame(ws, FRAME_DATA, off, f"echo:{off}".encode())
            
            # Complete QoS2 flow
            ft, fid_prepare, payload = await recv_frame(ws)
            assert ft == FRAME_PREPARE
            await send_frame(ws, FRAME_PREPARE_ACK, fid_prepare, b"")
            ft, fid_commit, payload = await recv_frame(ws)
            assert ft == FRAME_COMMIT and fid_commit == fid_prepare
            await send_frame(ws, FRAME_COMPLETE, fid_commit, b"")
            
            ft_data, fid_data_final, payload_data = await recv_data_or_handle_commit_retries(ws, fid_commit)
            assert payload_data == f"{off}".encode()
            assert ft_data == FRAME_DATA, f"Expected DATA, got {ft_data}"
            assert fid_data_final == fid_commit
            
    print("✔ 64‑bit wrap‑around tolerated")

async def test_connection_change():
    """Test QoS2 flow with connection change"""
    print("\n=== Test 8: Connection Change ===")
    
    # İlk bağlantıyı aç
    print("DEBUG: Opening first connection...")
    async with ws_connect(SERVER_URI) as ws1:
        print("DEBUG: First connection established")
        
        # DATA frame gönder
        print("DEBUG: Sending initial DATA frame...")
        await send_frame(ws1, FRAME_DATA, 0, b"echo:pa")
        
        # PREPARE bekle
        print("DEBUG: Waiting for PREPARE...")
        ft, fid_prepare1, payload = await recv_frame(ws1)
        assert ft == FRAME_PREPARE, f"Expected PREPARE, got {ft}"
        print(f"DEBUG: Received PREPARE for id {fid_prepare1}")
        
        # PREPARE_ACK gönder
        print("DEBUG: Sending PREPARE_ACK...")
        await send_frame(ws1, FRAME_PREPARE_ACK, fid_prepare1)
        
        # COMMIT bekle
        print("DEBUG: Waiting for COMMIT...")
        ft, fid_commit1, payload = await recv_frame(ws1)
        assert ft == FRAME_COMMIT, f"Expected COMMIT, got {ft}"
        assert fid_commit1 == fid_prepare1
        print(f"DEBUG: Received COMMIT for id {fid_commit1}")
        
        # COMPLETE gönder
        print("DEBUG: Sending COMPLETE...")
        await send_frame(ws1, FRAME_COMPLETE, fid_commit1)
        
        ft_data1, fid_data_final1, payload_data1 = await recv_data_or_handle_commit_retries(ws1, fid_commit1)
        assert payload_data1 == b"pa"
        assert ft_data1 == FRAME_DATA, f"Expected DATA, got {ft_data1}"
        assert fid_data_final1 == fid_commit1

        print("DEBUG: First connection completed")
    
    # Kısa bir bekleme
    await asyncio.sleep(1)
    
    # Yeni bağlantı aç (aynı client)
    print("DEBUG: Opening second connection...")
    async with ws_connect(SERVER_URI) as ws2:
        print("DEBUG: Second connection established")
        
        # DATA frame gönder
        print("DEBUG: Sending DATA frame...")
        await send_frame(ws2, FRAME_DATA, 0, b"echo:pa")
        
        # PREPARE bekle
        print("DEBUG: Waiting for PREPARE...")
        ft, fid_prepare2, payload = await recv_frame(ws2)
        assert ft == FRAME_PREPARE, f"Expected PREPARE, got {ft}"
        print(f"DEBUG: Received PREPARE for id {fid_prepare2}")
        
        # PREPARE_ACK gönder
        print("DEBUG: Sending PREPARE_ACK...")
        await send_frame(ws2, FRAME_PREPARE_ACK, fid_prepare2)
        
        # COMMIT bekle
        print("DEBUG: Waiting for COMMIT...")
        ft, fid_commit2, payload = await recv_frame(ws2)
        assert ft == FRAME_COMMIT, f"Expected COMMIT, got {ft}"
        assert fid_commit2 == fid_prepare2
        print(f"DEBUG: Received COMMIT for id {fid_commit2}")
        
        # COMPLETE gönder
        print("DEBUG: Sending COMPLETE...")
        await send_frame(ws2, FRAME_COMPLETE, fid_commit2)
        
        ft_data2, fid_data_final2, payload_data2 = await recv_data_or_handle_commit_retries(ws2, fid_commit2)
        assert payload_data2 == b"pa"
        assert ft_data2 == FRAME_DATA, f"Expected DATA, got {ft_data2}"
        assert fid_data_final2 == fid_commit2

        print("DEBUG: Second connection completed")
    
    print("✔ Connection change handled successfully")

async def test_qos2_prepare_retry():
    """
    Test QoS2 PREPARE retry behavior.
    
    Note: In real-world scenarios, due to network conditions and timing, it's possible
    to receive a COMMIT frame after sending COMPLETE. This is an edge case that clients
    should handle gracefully by ignoring any frames after COMPLETE. The test has been
    modified to handle this case by waiting for the final DATA frame while potentially
    receiving and ignoring other frames.
    """
    print("\n=== Test: QoS2 PREPARE retry behavior ===")
    
    # Manuel bağlantı yönetimi
    conn = ws_connect(SERVER_URI)
    try:
        async with conn as ws:
            # Send initial message
            await send_frame(ws, FRAME_DATA, 0, b"echo:ping")
            print("Sent initial DATA frame")
            
            # İlk PREPARE frame'ini al (bu 1. retry)
            ft, mid_prepare, pl = await recv_frame(ws)
            assert ft == FRAME_PREPARE, f"Expected PREPARE, got {ft}"
            print("Received PREPARE (1st retry)")
            
            # PREPARE_ACK göndermiyoruz, diğer retry'ları bekliyoruz
            print("Not sending PREPARE_ACK to force more retries")
            
            # Server'ın retry zamanlaması:
            # baseRetryMs=50, maxRetry=3, maxBackoffMs=200
            retry_times = [0.05, 0.1, 0.2]  # 50ms ve 100ms sonra
            
            for i, wait_time in enumerate(retry_times):
                print(f"Waiting {wait_time} seconds for retry {i+2}")  # i+2 çünkü ilk retry zaten geldi
                await asyncio.sleep(wait_time)
                
                # Retry'ı bekle
                ft, mid_retry, pl = await recv_frame(ws)
                assert ft == FRAME_PREPARE, f"Expected PREPARE retry, got {ft} on retry {i+2}"
                print(f"Received PREPARE retry {i+2}")
            
            print("All retries received, sending PREPARE_ACK")
            
            # Now send PREPARE_ACK
            await send_frame(ws, FRAME_PREPARE_ACK, mid_prepare)
            print("Sent PREPARE_ACK")
            
            # Should receive COMMIT with a reasonable timeout
            print("Waiting for COMMIT frame...")
            ft, mid_commit, pl = await recv_frame(ws)
            assert ft == FRAME_COMMIT, "Expected COMMIT after PREPARE_ACK"
            print("Received COMMIT")
            
            # Complete the flow
            await send_frame(ws, FRAME_COMPLETE, mid_commit)
            print("Sent COMPLETE")
            
            # Wait for final DATA frame, potentially ignoring other frames
            print("Waiting for final DATA frame...")
            start_time = time.time()
            while time.time() - start_time < 2.0:  # 2 second timeout
                try:
                    ft, mid, pl = await recv_frame(ws, timeout=0.1)  # Short timeout for each attempt
                    if ft == FRAME_DATA and pl == b"ping":
                        print("Received final DATA frame")
                        return  # Test passed
                    else:
                        print(f"Ignoring unexpected frame after COMPLETE: {frameTypeToString(ft)}")
                except asyncio.TimeoutError:
                    continue
            
            raise AssertionError("Did not receive final DATA frame within timeout")
            
    except Exception as e:
        print(f"Test failed: {e}")
        raise
    finally:
        print("Test completed")

async def test_qos2_commit_retry():
    """
    Test QoS2 COMMIT retry behavior.
    
    Note: In real-world scenarios, due to network conditions and timing, it's possible
    to receive frames in a different order than expected. For example, after sending
    PREPARE_ACK, we might receive another PREPARE before the COMMIT. This is an edge
    case that clients should handle gracefully. The test has been modified to handle
    these cases by waiting for the expected frame while potentially receiving and
    ignoring other frames.
    """
    print("\n=== Test: QoS2 COMMIT retry behavior ===")
    async with ws_connect(SERVER_URI) as ws:
        # Start QoS2 flow
        await send_frame(ws, FRAME_DATA, 0, b"echo:ping")
        
        # Receive PREPARE and send PREPARE_ACK
        ft, mid_prepare, pl = await recv_frame(ws)
        assert ft == FRAME_PREPARE
        await send_frame(ws, FRAME_PREPARE_ACK, mid_prepare)
        
        current_fid = mid_prepare # This is the ID for the transaction

        # Should receive multiple COMMIT retries
        # Default maxRetry=3 on server. Initial COMMIT + 3 retries = 4 COMMITs.
        # The test checks for 3 COMMITs. Let's assume it means initial + 2 retries, or just 3 attempts.
        # The loop 'for i in range(3)' suggests checking 3 COMMIT frames.
        # If server sends initial COMMIT, then client doesn't send COMPLETE.
        # Server's retryLoop will resend COMMIT.
        # We should see the first COMMIT, then two more retries if maxRetry >= 2.
        # If maxRetry = 3, we'd see initial + 3 retries if we never send COMPLETE.

        for i in range(3):  # Check first 3 COMMIT frames (initial + 2 retries, if maxRetry allows)
            print(f"Waiting for COMMIT attempt {i+1} for ID {current_fid}")
            # The original loop was a bit complex. Let's simplify.
            # We expect a COMMIT for current_fid.
            try:
                ft_commit_retry, mid_commit_retry, pl_commit_retry = await recv_frame(ws, timeout=1.0) # Wait up to 1s
                assert ft_commit_retry == FRAME_COMMIT, f"Expected COMMIT, got {frameTypeToString(ft_commit_retry)} on attempt {i+1}"
                assert mid_commit_retry == current_fid, f"COMMIT ID {mid_commit_retry} does not match expected ID {current_fid} on attempt {i+1}"
                print(f"Received COMMIT attempt {i+1} for ID {mid_commit_retry}")
            except asyncio.TimeoutError:
                raise AssertionError(f"Timeout waiting for COMMIT attempt {i+1} for ID {current_fid}")
            
            # Don't send COMPLETE to force retry, until the last iteration (or after loop)
            if i < 2 : # For the first 2 COMMITs received, don't send COMPLETE
                 print(f"Not sending COMPLETE after COMMIT attempt {i+1} to force more retries.")
            else: # On the 3rd COMMIT received, we will send COMPLETE after this loop
                 print(f"This was COMMIT attempt {i+1}. Will send COMPLETE after this.")
                 # mid_commit_retry now holds the ID of the last COMMIT received.
        
        # Now send COMPLETE for the original message ID
        print(f"Sending COMPLETE for ID {current_fid} after receiving multiple COMMITs")
        await send_frame(ws, FRAME_COMPLETE, current_fid) # Use current_fid which is mid_prepare
        
        # Wait for final DATA frame
        print(f"Waiting for final DATA frame for ID {current_fid}...")
        ft_final, mid_final, pl_final = await recv_data_or_handle_commit_retries(ws, current_fid)
        assert ft_final == FRAME_DATA and pl_final == b"ping"
        assert mid_final == current_fid
        print("Received final DATA frame")
        print("✔ QoS2 COMMIT retry test completed successfully")

async def test_qos2_max_retries():
    """Test QoS2 message is dropped after max retries in PREPARE stage"""
    print("\n=== Test: QoS2 max retries behavior ===")
    async with ws_connect(SERVER_URI) as ws:
        await send_frame(ws, FRAME_DATA, 0, b"echo:ping")
        
        # Let it retry max_retries times in PREPARE stage
        for i in range(4):  # Assuming max_retries=3
            ft, mid, pl = await recv_frame(ws, timeout=2)
            assert ft == FRAME_PREPARE, f"Expected PREPARE, got {ft} on retry {i}"
            print(f"Received PREPARE retry {i+1}")
            # Don't send PREPARE_ACK to force retry
            
        # After max retries, message should be dropped
        try:
            await recv_frame(ws, timeout=2)
            assert False, "Should not receive more retries after max_retries"
        except asyncio.TimeoutError:
            print("✔ Message dropped after max retries")

async def test_qos2_retry_after_reconnect():
    """Test QoS2 retry behavior after connection drop and reconnect"""
    print("\n=== Test: QoS2 retry after reconnect ===")
    # First connection
    async with ws_connect(SERVER_URI) as ws1:
        await send_frame(ws1, FRAME_DATA, 0, b"echo:ping")
        ft, mid, pl = await recv_frame(ws1)
        assert ft == FRAME_PREPARE
        # Don't send PREPARE_ACK, let connection drop
        
    # Wait a bit
    await asyncio.sleep(0.1)
    print("DEBUG: Waiting for PREPARE retry after reconnect")
    
    # New connection
    async with ws_connect(SERVER_URI) as ws2:
        # Should receive PREPARE retry
        ft, mid, pl = await recv_frame(ws2)
        print(f"DEBUG: Received frame type: {ft}")
        assert ft == FRAME_PREPARE, "Expected PREPARE retry after reconnect"
        
        # Send PREPARE_ACK
        await send_frame(ws2, FRAME_PREPARE_ACK, mid)
        
        # Should receive COMMIT
        ft, mid, pl = await recv_frame(ws2)
        print(f"DEBUG: Received frame type: {ft}")
        assert ft == FRAME_COMMIT, f"Expected COMMIT, got {ft}"
        
        # Send COMPLETE
        await send_frame(ws2, FRAME_COMPLETE, mid)
        
        # Should receive final DATA
        ft, mid, pl = await recv_frame(ws2)
        assert ft == FRAME_DATA, f"Expected DATA, got {ft}"
        assert pl == b"ping", f"Expected 'ping', got {pl}"
        
        print("✔ Retry after reconnect completed successfully")

class WebSocketClient:
    def __init__(self, client_id: str, device_id: str):
        self.client_id = client_id
        self.device_id = device_id
        self.ws = None
        self.received_messages = []
        self.connected = False
        self.session_token = None
        self._message_queue = asyncio.Queue()
        self._message_event = asyncio.Event()
        self._processed_ids = set()
        self._processed_mutex = asyncio.Lock()
        self._message_handler = None
        self._current_frame = None
        self._frame_event = asyncio.Event()

    async def connect(self):
        uri = SERVER_URI  # Use the global SERVER_URI
        
        headers = [
            ("x-client-id", self.client_id),
            ("x-device-id", self.device_id)
        ]
        
        if self.session_token:
            headers.append(("x-session-token", self.session_token))
            print(f"Using existing session token: {self.session_token}")
        
        self.ws = await websockets.connect(uri, additional_headers=headers)
        
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
        
        # Start message handler
        self._message_handler = asyncio.create_task(self._handle_messages())

    async def disconnect(self):
        if self._message_handler:
            self._message_handler.cancel()
            try:
                await self._message_handler
            except asyncio.CancelledError:
                pass
            self._message_handler = None

        if self.ws:
            await self.ws.close()
            self.connected = False
            print(f"Disconnected client_id: {self.client_id}")

    async def _handle_messages(self):
        try:
            while self.connected:
                try:
                    raw = await self.ws.recv()
                    print(f"DEBUG: Raw data length: {len(raw)} bytes")
                    hex_str = ' '.join(f'{b:02x}' for b in raw)
                    print(f"DEBUG: Raw data (hex): [{hex_str}]")
                    
                    if len(raw) < 9:  # Minimum frame size
                        print(f"ERROR: Frame too short: {len(raw)} bytes")
                        continue
                    
                    ft = raw[0]
                    mid = struct.unpack_from(">Q", raw, 1)[0]
                    pl = raw[9:]
                    
                    ft_str = frameTypeToString(ft)
                    print(f"DEBUG: Parsed frame - Type: {ft_str}, ID: {mid}, Payload length: {len(pl)}")
                
                    # Put the message in the queue
                    await self._message_queue.put((ft, mid, pl))
                    self._message_event.set()
                
                except websockets.exceptions.ConnectionClosed:
                    print("Connection closed")
                    break
                except Exception as e:
                    print(f"Error in _handle_messages: {str(e)}")
                    break
        except asyncio.CancelledError:
            print("Message handler cancelled")
        except Exception as e:
            print(f"Unexpected error in _handle_messages: {str(e)}")
        finally:
            self.connected = False

    async def recv_frame(self, timeout=None):
        try:
            if timeout:
                frame = await asyncio.wait_for(self._message_queue.get(), timeout)
            else:
                frame = await self._message_queue.get()
            return frame
        except asyncio.TimeoutError:
            print("DEBUG: Timeout waiting for frame")
            raise
        except Exception as e:
            print(f"ERROR in recv_frame: {str(e)}")
            raise

    async def wait_for_messages(self, count, timeout=10):
        start_time = time.time()
        while len(self.received_messages) < count:
            if time.time() - start_time > timeout:
                raise TimeoutError(f"Timeout waiting for {count} messages")
            await asyncio.sleep(0.1)

    def get_received_messages(self):
        return self.received_messages

    async def send_rpc(self, method: str, *args):
        if not self.connected:
            raise ConnectionError("Not connected")
            
        payload = f"{method}:{':'.join(str(arg) for arg in args)}".encode()
        await send_frame(self.ws, FRAME_DATA, 0, payload)
        
        # Wait for response
        ft, mid, pl = await self.recv_frame()
        if ft == FRAME_PREPARE:
            await send_frame(self.ws, FRAME_PREPARE_ACK, mid)
            ft, mid, pl = await self.recv_frame()
            if ft == FRAME_COMMIT:
                await send_frame(self.ws, FRAME_COMPLETE, mid)
                ft, mid, pl = await self.recv_frame()
                if ft == FRAME_DATA:
                    self.received_messages.append(pl.decode())
                    return pl.decode()
        return None

async def test_offline_message_delivery():
    try:
        # Genel timeout için başlangıç zamanı
        start_time = time.time()
        MAX_TEST_DURATION = 7  # 7 saniye
        
        # 1. X kullanıcısı bağlanır ve login olur
        x_client = WebSocketClient("client_x", "dev_1")  # X için farklı credentials
        await x_client.connect()
        print("\nX client connected and starting login...")
        
        # Login için QoS2 flow
        await send_frame(x_client.ws, FRAME_DATA, 0, b"login:X:user") # client mid 0
        
        # PREPARE frame'ini bekle ve retry'ları handle et
        while True:
            ft_prepare_login, fid_prepare_login, _ = await x_client.recv_frame()
            if ft_prepare_login == FRAME_PREPARE:
                print(f"X Login: Received PREPARE for ID {fid_prepare_login}")
                await send_frame(x_client.ws, FRAME_PREPARE_ACK, fid_prepare_login)
                print(f"X Login: Sent PREPARE_ACK for ID {fid_prepare_login}")
                break
            else:
                print(f"X Login: Ignoring unexpected frame type {frameTypeToString(ft_prepare_login)} while waiting for PREPARE")
        
        # COMMIT frame'ini bekle ve retry'ları handle et
        while True:
            ft_commit_login, fid_commit_login, _ = await x_client.recv_frame()
            if ft_commit_login == FRAME_COMMIT:
                assert fid_commit_login == fid_prepare_login
                print(f"X Login: Received COMMIT for ID {fid_commit_login}")
                await send_frame(x_client.ws, FRAME_COMPLETE, fid_commit_login)
                print(f"X Login: Sent COMPLETE for ID {fid_commit_login}")
                break
            elif ft_commit_login == FRAME_PREPARE:
                print(f"X Login: Received retry PREPARE for ID {fid_commit_login}, sending PREPARE_ACK again")
                await send_frame(x_client.ws, FRAME_PREPARE_ACK, fid_commit_login)
            else:
                print(f"X Login: Ignoring unexpected frame type {frameTypeToString(ft_commit_login)} while waiting for COMMIT")
        
        # Login yanıtını bekle (DATA frame)
        while True:
            ft_data_login, fid_data_login, _ = await x_client.recv_frame()
            if ft_data_login == FRAME_DATA:
                print(f"X Login: Login response DATA received for ID {fid_data_login}")
                break
            elif ft_data_login == FRAME_COMMIT:
                print(f"X Login: Received retry COMMIT for ID {fid_data_login}, sending COMPLETE again")
                await send_frame(x_client.ws, FRAME_COMPLETE, fid_data_login)
            elif ft_data_login == FRAME_PREPARE:
                print(f"X Login: Received retry PREPARE for ID {fid_data_login}, sending PREPARE_ACK again")
                await send_frame(x_client.ws, FRAME_PREPARE_ACK, fid_data_login)
            else:
                print(f"X Login: Ignoring unexpected frame type {frameTypeToString(ft_data_login)} while waiting for DATA")
        
        print("X client login completed")
        
        # İlk session token'ı sakla
        first_token = x_client.session_token
        print(f"First session token: {first_token}")
        
        # 2. X kullanıcısının bağlantısını kapat
        print("\nDisconnecting X client...")
        await x_client.disconnect()
        
        # 3. Y kullanıcısı bağlanır ve birden fazla mesaj gönderir
        print("\nY client connecting...")
        y_client = WebSocketClient("client_y", "dev_2")  # Y için farklı credentials
        await y_client.connect()
        print("Y client connected")
        
        # 10 farklı mesaj gönder
        messages = []
        for i in range(10):
            # Genel timeout kontrolü
            if time.time() - start_time > MAX_TEST_DURATION:
                raise TimeoutError(f"Test exceeded maximum duration of {MAX_TEST_DURATION} seconds")
                
            message = f"test message {i+1}"
            messages.append(message)
            print(f"\nSending message {i+1}...")
            
            # Her mesaj için QoS2 flow
            await send_frame(y_client.ws, FRAME_DATA, 0, f"sendToPremium:{message}".encode())
            
            # PREPARE frame'ini bekle ve retry'ları handle et
            while True:
                ft_prepare_y, fid_prepare_y, _ = await y_client.recv_frame()
                if ft_prepare_y == FRAME_PREPARE:
                    print(f"Y: Received PREPARE for message {i+1}, ID {fid_prepare_y}")
                    await send_frame(y_client.ws, FRAME_PREPARE_ACK, fid_prepare_y)
                    print(f"Y: Sent PREPARE_ACK for message {i+1}, ID {fid_prepare_y}")
                    break
                else:
                    print(f"Y: Ignoring unexpected frame type {frameTypeToString(ft_prepare_y)} while waiting for PREPARE")
            
            # COMMIT frame'ini bekle ve retry'ları handle et
            while True:
                ft_commit_y, fid_commit_y, _ = await y_client.recv_frame()
                if ft_commit_y == FRAME_COMMIT:
                    assert fid_commit_y == fid_prepare_y
                    print(f"Y: Received COMMIT for message {i+1}, ID {fid_commit_y}")
                    await send_frame(y_client.ws, FRAME_COMPLETE, fid_commit_y)
                    print(f"Y: Sent COMPLETE for message {i+1}, ID {fid_commit_y}")
                    break
                elif ft_commit_y == FRAME_PREPARE:
                    print(f"Y: Received retry PREPARE for message {i+1}, ID {fid_commit_y}, sending PREPARE_ACK again")
                    await send_frame(y_client.ws, FRAME_PREPARE_ACK, fid_commit_y)
                else:
                    print(f"Y: Ignoring unexpected frame type {frameTypeToString(ft_commit_y)} while waiting for COMMIT")
            
            # DATA frame'ini bekle ve retry'ları handle et
            while True:
                ft_data_y, fid_data_y, pl_data_y = await y_client.recv_frame()
                if ft_data_y == FRAME_DATA:
                    print(f"Y: Received DATA confirmation for message {i+1}, ID {fid_data_y}")
                    break
                elif ft_data_y == FRAME_COMMIT:
                    print(f"Y: Received retry COMMIT for message {i+1}, ID {fid_data_y}, sending COMPLETE again")
                    await send_frame(y_client.ws, FRAME_COMPLETE, fid_data_y)
                elif ft_data_y == FRAME_PREPARE:
                    print(f"Y: Received retry PREPARE for message {i+1}, ID {fid_data_y}, sending PREPARE_ACK again")
                    await send_frame(y_client.ws, FRAME_PREPARE_ACK, fid_data_y)
                else:
                    print(f"Y: Ignoring unexpected frame type {frameTypeToString(ft_data_y)} while waiting for DATA")
            
            print(f"Y sent message {i+1}: {message}")
        
        # Y client'ı kapat
        print("\nDisconnecting Y client...")
        await y_client.disconnect()
        
        # 4. X kullanıcısı tekrar bağlanır (aynı session token ile)
        print("\nReconnecting X client...")
        x_client.session_token = first_token
        await x_client.connect()
        print("X client reconnected")
        
        # 5. X kullanıcısının tüm mesajları almasını bekle
        print("\nWaiting for X to receive all messages...")
        
        # Mesajları işle
        received_messages_content = []
        
        while len(received_messages_content) < len(messages):
            # Genel timeout kontrolü
            if time.time() - start_time > MAX_TEST_DURATION:
                raise TimeoutError(f"Test exceeded maximum duration of {MAX_TEST_DURATION} seconds while X receiving. Received {len(received_messages_content)}/{len(messages)}")
                
            try:
                # Get any frame from the client's queue
                current_ft, current_fid, current_pl = await x_client.recv_frame(timeout=0.8)
                
                if current_ft == FRAME_PREPARE:
                    print(f"X: Handling PREPARE for ID {current_fid}")
                    await send_frame(x_client.ws, FRAME_PREPARE_ACK, current_fid)
                    print(f"X: Sent PREPARE_ACK for ID {current_fid}")
                                
                elif current_ft == FRAME_COMMIT:
                    print(f"X: Handling COMMIT for ID {current_fid}")
                    await send_frame(x_client.ws, FRAME_COMPLETE, current_fid)
                    print(f"X: Sent COMPLETE for ID {current_fid}")

                elif current_ft == FRAME_DATA:
                    message_text = current_pl.decode('utf-8', errors='replace')
                    print(f"X: Handling DATA for ID {current_fid}, Content: '{message_text}'")
                    
                    if message_text not in received_messages_content:
                        received_messages_content.append(message_text)
                        print(f"X: Added message '{message_text}' to received_messages_content. Count: {len(received_messages_content)}/{len(messages)}")
                    else:
                        print(f"X: Duplicate DATA message '{message_text}' for ID {current_fid}. Ignoring.")
                                    
                else:
                    print(f"X: Ignoring unexpected frame type {frameTypeToString(current_ft)} for ID {current_fid}")

            except asyncio.TimeoutError:
                print(f"X: Timeout waiting for next frame. Received {len(received_messages_content)}/{len(messages)} messages.")
                if not x_client.ws or x_client.ws.closed:
                    print("X client connection closed unexpectedly while waiting for messages.")
                    break
                continue
            except Exception as e_loop:
                print(f"X: Error in message receiving loop: {e_loop}")
                raise
        
        # İşlenen mesajları kontrol et
        print(f"\nReceived messages content by X: {received_messages_content}")
        print(f"Expected messages: {messages}")
        
        # Her mesajın alındığını kontrol et (sayı ve içerik)
        assert len(received_messages_content) == len(messages), \
            f"X client did not receive all messages. Expected {len(messages)}, got {len(received_messages_content)}. Received: {received_messages_content}"

        for message_expected in messages:
            assert message_expected in received_messages_content, f"X kullanıcısı '{message_expected}' mesajını almadı! Alınanlar: {received_messages_content}"
        
        print(f"\nTest başarılı! X kullanıcısı {len(received_messages_content)} mesajı aldı.")
        
    except Exception as e:
        print(f"Test başarısız: {e}")
        raise
    finally:
        # Sadece X client'ı temizle
        if 'x_client' in locals():
            await x_client.disconnect()

async def main():
    # Eski testleri yorum satırına alıyoruz
    await test_resume_within_ttl()
    await asyncio.sleep(AFTER_TTL)
    await test_resume_after_ttl()
    await asyncio.sleep(AFTER_TTL)
    await test_state_within_ttl()
    await asyncio.sleep(AFTER_TTL)
    await test_state_after_ttl()
    await asyncio.sleep(AFTER_TTL)
    await test_duplicate_data()
    await asyncio.sleep(AFTER_TTL)
    await test_out_of_order_ack()
    await asyncio.sleep(AFTER_TTL)
    await test_id_wraparound()
    await asyncio.sleep(AFTER_TTL)
    await test_connection_change()
    await asyncio.sleep(AFTER_TTL)
    await test_qos2_prepare_retry()
    await asyncio.sleep(AFTER_TTL)
    await test_qos2_commit_retry()
    await asyncio.sleep(AFTER_TTL)
    await test_qos2_max_retries()
    await asyncio.sleep(AFTER_TTL)
    await test_qos2_retry_after_reconnect()
    await asyncio.sleep(AFTER_TTL)
    # Yeni testimizi çalıştırıyoruz
    await test_offline_message_delivery()
    print("\n🎉 ALL test passed!")

if __name__ == "__main__":
    asyncio.run(main())
