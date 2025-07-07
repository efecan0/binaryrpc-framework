import asyncio, websockets, json
import random

URI = "ws://127.0.0.1:9000"

def pack(method, payload=""):
    """SimpleTextProtocol çerçevesi: method:payload  – bytes olarak döner."""
    return f"{method}:{payload}".encode()

def make_headers():
    cid = f"cli-{random.randint(1000, 9999)}"
    did = f"dev-{random.randint(1000, 9999)}"
    return [
        ("x-client-id", cid),
        ("x-device-id", did)
    ]

async def run():
    # ---- 1. ana bağlantı  ----
    async with websockets.connect(URI, additional_headers=make_headers()) as ws:
        await ws.send(pack("set.nonidx", "Jane"))
        print("set.nonidx ➜", (await ws.recv()).decode())

        await ws.send(pack("get.nonidx"))
        print("get.nonidx ➜", (await ws.recv()).decode())

        await ws.send(pack("find.city", "Paris"))
        print("find.city(Paris, boş) ➜", (await ws.recv()).decode())

        await ws.send(pack("set.idx", "Paris"))
        print("set.idx ➜", (await ws.recv()).decode())

        await ws.send(pack("find.city", "Paris"))
        print("find.city(Paris, 1) ➜", (await ws.recv()).decode())

        await ws.send(pack("list.sessions"))
        first_count = (await ws.recv()).decode()
        print("list.sessions (should be 1) ➜", first_count)

        # bağlantıyı kopar, session silinsin
        await ws.send(pack("bye"))
    await asyncio.sleep(2)
    # ---- 2. yeni bağlantı (eski session yok) ----
    async with websockets.connect(URI, additional_headers=make_headers()) as ws2:
        await ws2.send(pack("find.city", "Paris"))
        print("find.city(Paris, 0) ➜", (await ws2.recv()).decode())

        await ws2.send(pack("list.sessions"))
        print("list.sessions (should be 1) ➜", (await ws2.recv()).decode())

asyncio.run(run())
