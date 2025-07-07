import asyncio
import websockets

async def error_propagation_test():
    uri = "ws://localhost:9002"
    headers = {
        "x-client-id": "1",
        "x-device-id": "dev_1"
    }

    async with websockets.connect(uri, additional_headers=headers) as websocket:
        # Test 1: invalid method
        print("[TEST] Sending invalid method...")
        await websocket.send(b"invalid_method:xyz")
        resp1 = await websocket.recv()
        resp1_str = resp1.decode()
        print(f"Response (invalid method): {resp1_str}")
        assert "error" in resp1_str.lower(), "Expected error for invalid method"

        # Test 2: throwing handler
        print("[TEST] Calling throwing_handler...")
        await websocket.send(b"throwing_handler:test")
        resp2 = await websocket.recv()
        resp2_str = resp2.decode()
        print(f"Response (throwing handler): {resp2_str}")
        assert "error" in resp2_str.lower(), "Expected error for throwing handler"

        # Test 3: protocol parse error (invalid format)
        print("[TEST] Sending invalid protocol message...")
        await websocket.send(b"gibberish_that_doesnt_match:protocol")
        resp3 = await websocket.recv()
        resp3_str = resp3.decode()
        print(f"Response (parse error): {resp3_str}")
        assert "error" in resp3_str.lower(), "Expected error for protocol parse error"

        print("[TEST] SUCCESS: Error propagation test passed!")

asyncio.run(error_propagation_test())
