import asyncio, websockets, json
import random

URI = "ws://127.0.0.1:9000"

def pack(method, payload=""):
    """SimpleTextProtocol framework: method:payload – returns as bytes."""
    return f"{method}:{payload}".encode()

def make_headers():
    cid = f"cli-{random.randint(1000, 9999)}"
    did = f"dev-{random.randint(1000, 9999)}"
    return [
        ("x-client-id", cid),
        ("x-device-id", did)
    ]

async def run():
    # ---- 1. main connection ----
    async with websockets.connect(URI, additional_headers=make_headers()) as ws:
        await ws.send(pack("set.nonidx", "Jane"))
        print("set.nonidx ➜", (await ws.recv()).decode())

        await ws.send(pack("get.nonidx"))
        print("get.nonidx ➜", (await ws.recv()).decode())

        await ws.send(pack("find.city", "Paris"))
        print("find.city(Paris, empty) ➜", (await ws.recv()).decode())

        await ws.send(pack("set.idx", "Paris"))
        print("set.idx ➜", (await ws.recv()).decode())

        await ws.send(pack("find.city", "Paris"))
        print("find.city(Paris, 1) ➜", (await ws.recv()).decode())

        await ws.send(pack("list.sessions"))
        first_count = (await ws.recv()).decode()
        print("list.sessions (should be 1) ➜", first_count)

        # disconnect, session should be deleted
        await ws.send(pack("bye"))
    await asyncio.sleep(2)
    # ---- 2. new connection (no old session) ----
    async with websockets.connect(URI, additional_headers=make_headers()) as ws2:
        await ws2.send(pack("find.city", "Paris"))
        print("find.city(Paris, 0) ➜", (await ws2.recv()).decode())

        await ws2.send(pack("list.sessions"))
        print("list.sessions (should be 1) ➜", (await ws2.recv()).decode())

asyncio.run(run())
