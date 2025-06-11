# proxy_server.py
import sys
from aiohttp import web, WSMsgType

sys.path.append('..')
from settings import PORT

routes = web.RouteTableDef()
clients = set()
source_ws = None  # current audio sender

@routes.get("/recording")
async def handle_input(request):
    global source_ws
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    source_ws = ws
    print("🎙️ Microphone client connected")

    try:
        async for msg in ws:
            if msg.type == WSMsgType.BINARY:
                for client in list(clients):
                    try:
                        await client.send_bytes(msg.data)
                    except Exception:
                        clients.discard(client)
    finally:
        if source_ws is ws:
            source_ws = None
        print("❌ Microphone client disconnected")
    return ws

@routes.get("/listening")
async def handle_output(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    clients.add(ws)
    print("🔊 Speaker client connected")

    try:
        async for _ in ws:
            pass
    finally:
        clients.remove(ws)
        print("❌ Speaker client disconnected")
    return ws

app = web.Application()
app.add_routes(routes)

if __name__ == "__main__":
    web.run_app(app, port=PORT)
