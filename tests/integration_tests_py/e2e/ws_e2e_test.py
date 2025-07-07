#!/usr/bin/env python3
#main.cpp
import asyncio
import websockets

SERVER_HOST = "localhost"
SERVER_PORT = 9011
SERVER_URI = f"ws://{SERVER_HOST}:{SERVER_PORT}"

HEADERS = {
    "x-client-id": "1",
    "x-device-id": "dev_1"
}

def print_result(name, ok):
    print(f"[{'OK' if ok else 'FAIL'}] {name}")

async def test_echo_simple_text():
    try:
        async with websockets.connect(SERVER_URI, open_timeout=3, additional_headers=HEADERS) as ws:
            await ws.send(b"echo:hello world")
            resp = await ws.recv()
            if not isinstance(resp, (bytes, bytearray)):
                raise Exception("echo: response is not bytes")
            if resp != b"hello world":
                raise Exception(f"echo: unexpected response: {resp}")
        print_result("echo_simple_text", True)
    except Exception as e:
        print_result("echo_simple_text", False)
        raise

async def test_error_simple_text():
    try:
        async with websockets.connect(SERVER_URI, open_timeout=3, additional_headers=HEADERS) as ws:
            await ws.send(b"unknown:payload")
            resp = await ws.recv()
            if not isinstance(resp, (bytes, bytearray)):
                raise Exception("error: response is not bytes")
            if not resp.startswith(b"error:"):
                raise Exception(f"error: does not start with 'error:': {resp}")
            if not resp.startswith(b"error:3:"):
                raise Exception(f"error: does not start with 'error:3:': {resp}")
        print_result("error_simple_text", True)
    except Exception as e:
        print_result("error_simple_text", False)
        raise

async def main():
    await test_echo_simple_text()
    await test_error_simple_text()
    print("E2E tests passed")

if __name__ == "__main__":
    asyncio.run(main())
