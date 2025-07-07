import asyncio
import websockets

async def session_persistence_test():
    uri = "ws://localhost:9000"
    headers = {
        "x-client-id": "1",
        "x-device-id": "dev_1"
    }

    async with websockets.connect(uri, additional_headers=headers) as websocket:
        print("[TEST] Sending set_data_indexed_false...")
        await websocket.send(b"set_data_indexed_false:brocan")
        resp1 = await websocket.recv()
        resp1_str = resp1.decode()
        print(f"Response: {resp1_str}")
        assert resp1_str == "OK", "set_data_indexed_false failed"

        print("[TEST] Sending get_data...")
        await websocket.send(b"get_data:")
        resp2 = await websocket.recv()
        resp2_str = resp2.decode()
        print(f"Response: {resp2_str}")
        assert resp2_str == "brocan", "get_data after indexed_false failed"

        print("[TEST] Sending find_by...")
        await websocket.send(b"find_by:brocan")
        resp3 = await websocket.recv()
        resp3_str = resp3.decode()
        print(f"Response: '{resp3_str}'")
        assert resp3_str.strip() == "", "find_by should return empty before indexed_true"

    async with websockets.connect(uri, additional_headers=headers) as websocket:
        print("[TEST] Sending set_data_indexed_true...")
        await websocket.send(b"set_data_indexed_true:brocan")
        resp4 = await websocket.recv()
        resp4_str = resp4.decode()
        print(f"Response: {resp4_str}")
        assert resp4_str == "OK", "set_data_indexed_true failed"

        print("[TEST] Sending find_by again...")
        await websocket.send(b"find_by:brocan")
        resp5 = await websocket.recv()
        resp5_str = resp5.decode()
        print(f"Response: '{resp5_str}'")
        assert resp5_str.startswith("S"), "find_by should return session ID after indexed_true"

        print("[TEST] SUCCESS: Session persistency test passed!")

asyncio.run(session_persistence_test())
