import asyncio
import websockets

async def test_rpc(method, payload):
    uri = "ws://127.0.0.1:9000"
    headers = {
        "x-client-id": "1",
        "x-device-id": "dev_1"
    }
    async with websockets.connect(uri, additional_headers=headers) as websocket:
        message = f"{method}:{payload}"
        binary_message = message.encode('utf-8')

        print(f"[Client] Sending binary frame: {binary_message}")
        await websocket.send(binary_message)

        response = await websocket.recv()

        if isinstance(response, bytes):
            decoded_response = response.decode('utf-8')
            print(f"[Client ✅] Binary response received: {decoded_response}")
        else:
            print(f"[Client ⚠️] Text response received: {response}")

# Normal başarılı çağrı
asyncio.run(test_rpc("login", "test payload"))
asyncio.run(test_rpc("test.middleware", "another payload"))

# next() atlanmış zincir
asyncio.run(test_rpc("stuck.method", "should fail"))

# middleware throw fırlatan
asyncio.run(test_rpc("throw.method", "should throw"))
