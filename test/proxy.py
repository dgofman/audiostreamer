import sys
from aiohttp import web, WSMsgType
from collections import defaultdict

sys.path.append('..')
from utils import PORT

# Configuration
FILTER_SELF_AUDIO = False  # Set to True to prevent microphone echo to same clientId

# Data structures
rooms = defaultdict(lambda: {"microphones": set(), "speakers": dict()})

routes = web.RouteTableDef()

@routes.get("/ws")
async def websocket_handler(request):
    room_id = request.query.get("roomId")
    client_id = request.query.get("clientId")
    role = request.query.get("role")

    if not room_id or not client_id or not role:
        return web.Response(status=400, text="Missing roomId, clientId, or role")

    ws = web.WebSocketResponse()
    await ws.prepare(request)

    room = rooms[room_id]
    is_mic = 'recording' in role
    is_speaker = 'listening' in role

    if is_mic:
        room["microphones"].add(ws)
        print(f"🎙️ Microphone joined room {room_id}, clientId={client_id}")
    if is_speaker:
        room["speakers"][ws] = client_id
        print(f"🔊 Speaker joined room {room_id}, clientId={client_id}")

    try:
        async for msg in ws:
            if msg.type == WSMsgType.BINARY and is_mic:
                # Broadcast to all speakers in the same room
                for speaker_ws, speaker_client_id in list(room["speakers"].items()):
                    if FILTER_SELF_AUDIO and speaker_client_id == client_id:
                        continue
                    try:
                        await speaker_ws.send_bytes(msg.data)
                    except Exception:
                        room["speakers"].pop(speaker_ws, None)
            elif msg.type == WSMsgType.TEXT:
                pass  # Optional: handle text messages if needed
    finally:
        if is_mic:
            room["microphones"].discard(ws)
            print(f"❌ Microphone left room {room_id}, clientId={client_id}")
        if is_speaker:
            room["speakers"].pop(ws, None)
            print(f"❌ Speaker left room {room_id}, clientId={client_id}")

        # Clean up room if empty
        if not room["microphones"] and not room["speakers"]:
            del rooms[room_id]

    return ws

# App setup
app = web.Application()
app.add_routes(routes)

if __name__ == "__main__":
    print(f"🔧 FILTER_SELF_AUDIO = {FILTER_SELF_AUDIO}")
    if not FILTER_SELF_AUDIO:
        print("⚠️ This may cause a feedback loop if the same client sends and receives audio.")
        print("👉 It's recommended to set FILTER_SELF_AUDIO = True for most setups.")
    web.run_app(app, port=PORT)
