#advanced_general_server.cpp
import asyncio
import websockets

async def client_task(client_id, port=9000):
    uri = f"ws://localhost:{port}"
    username = f"user_{client_id}"

    # Prepare headers
    headers = {
        "x-client-id": str(client_id),
        "x-device-id": f"dev_{client_id}"
        # "x-session-token": "...",  # Add if necessary
    }

    async with websockets.connect(uri, additional_headers=headers) as websocket:
        print(f"[Client {client_id}] Connecting...")

        # STEP 1: set_data_indexed_true
        msg1 = f"set_data_indexed_true:{username}".encode()
        await websocket.send(msg1)
        resp1 = await websocket.recv()
        resp1_str = resp1.decode()
        print(f"[Client {client_id}] set_data_indexed_true response: {resp1_str}")
        assert resp1_str == "OK", f"Client {client_id} set_data_indexed_true failed"

        # STEP 2: get_data
        await websocket.send(b"get_data:")
        resp2 = await websocket.recv()
        resp2_str = resp2.decode()
        print(f"[Client {client_id}] get_data response: {resp2_str}")
        assert resp2_str == username, f"Client {client_id} get_data mismatch"

        # STEP 3: find_by
        msg3 = f"find_by:{username}".encode()
        await websocket.send(msg3)
        resp3 = await websocket.recv()
        resp3_str = resp3.decode()
        print(f"[Client {client_id}] find_by response: {resp3_str}")
        assert resp3_str.startswith("S"), f"Client {client_id} find_by failed"

        print(f"[Client {client_id}] SUCCESS")

async def concurrency_test(num_clients=10):
    tasks = [client_task(i) for i in range(num_clients)]
    await asyncio.gather(*tasks)

asyncio.run(concurrency_test(10))
