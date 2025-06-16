import sys
import asyncio
from aiohttp import web, WSMsgType
from collections import defaultdict, deque
import json

sys.path.append('..')
from utils import PATH, PORT, SAMPLE_RATE, CHANNELS, BYTE_DEPTH

# Configurable options:
FILTER_SELF_AUDIO = False  # Prevent microphone echo to same clientId

# Constants:
BYTES_PER_MS = SAMPLE_RATE * CHANNELS * BYTE_DEPTH / 1000
JITTER_MIN_MS = 50
JITTER_MAX_MS = 300
TARGET_BLOCK_MS = 20
JITTER_MARGIN = 20

rooms = defaultdict(lambda: {
    "microphones": set(),
    "speakers": dict(),
    "buffer": bytearray(),
    "flush_task": None,
    "jitter_smoother": deque(maxlen=5),
    "last_jitter_sent": (JITTER_MIN_MS, JITTER_MAX_MS),
})

routes = web.RouteTableDef()

def calculate_latency_ms(room):
    return len(room["buffer"]) / BYTES_PER_MS

def estimate_jitter(latency):
    if latency < 100:
        return (80, 250)
    elif latency < 200:
        return (100, 300)
    else:
        return (120, 350)

def smooth_jitter(room, jitter):
    room["jitter_smoother"].append(jitter)
    smoothed_min = int(sum(x[0] for x in room["jitter_smoother"]) / len(room["jitter_smoother"]))
    smoothed_max = int(sum(x[1] for x in room["jitter_smoother"]) / len(room["jitter_smoother"]))
    return (smoothed_min, smoothed_max)

async def flush_loop(room_id):
    room = rooms[room_id]
    try:
        while True:
            latency = calculate_latency_ms(room)
            block_size = int(BYTES_PER_MS * TARGET_BLOCK_MS)

            jitter_raw = estimate_jitter(latency)
            jitter_smoothed = smooth_jitter(room, jitter_raw)

            if jitter_smoothed != room["last_jitter_sent"]:
                room["last_jitter_sent"] = jitter_smoothed
                message = json.dumps({"jitterMinMs": jitter_smoothed[0], "jitterMaxMs": jitter_smoothed[1]})
                for speaker_ws in list(room["speakers"].keys()):
                    try:
                        await speaker_ws.send_str(message)
                    except:
                        room["speakers"].pop(speaker_ws, None)

            # Safety: drop excessive buffer
            max_allowed_bytes = JITTER_MAX_MS * BYTES_PER_MS
            if len(room["buffer"]) > max_allowed_bytes:
                drop_bytes = int(len(room["buffer"]) - max_allowed_bytes)
                room["buffer"] = room["buffer"][drop_bytes:]

            while len(room["buffer"]) >= block_size:
                chunk = bytes(room["buffer"][:block_size])
                room["buffer"] = room["buffer"][block_size:]
                for speaker_ws, speaker_id in list(room["speakers"].items()):
                    try:
                        # Self-filter logic:
                        if FILTER_SELF_AUDIO and any(mid == speaker_id for _, mid in room["microphones"]):
                            continue
                        await speaker_ws.send_bytes(chunk)
                    except:
                        room["speakers"].pop(speaker_ws, None)

            if latency > 400:  # only print abnormal situations
                print(f"⚠ High buffer: {int(latency)}ms [{len(room['buffer'])}B]")

            await asyncio.sleep(0.01)
    except asyncio.CancelledError:
        print(f"🚪 Flush task for room {room_id} terminated")

@routes.get(f"/{PATH}")
async def websocket_handler(request):
    room_id = request.query.get("roomId", "0")
    client_id = request.query.get("clientId", "unknown")
    role = request.query.get("role", "")

    if not room_id or not client_id or not role:
        return web.Response(status=400, text="Missing params")

    ws = web.WebSocketResponse()
    await ws.prepare(request)

    room = rooms[room_id]
    is_mic = 'recording' in role
    is_speaker = 'listening' in role

    if is_mic:
        room["microphones"].add((ws, client_id))
        print(f"🎙️ Microphone connected: {client_id}")

    if is_speaker:
        room["speakers"][ws] = client_id
        print(f"🔊 Speaker connected: {client_id}")

    if is_mic and not room["flush_task"]:
        room["flush_task"] = asyncio.create_task(flush_loop(room_id))
        print(f"🚀 Started flush task for room {room_id}")

    try:
        async for msg in ws:
            if msg.type == WSMsgType.BINARY and is_mic:
                room["buffer"].extend(msg.data)
    except Exception as e:
        print(f"❌ WebSocket error: {e}")
    finally:
        if is_mic:
            print(f"❌ Microphone left room {room_id}, clientId={client_id}")
            room["microphones"].discard((ws, client_id))
        if is_speaker:
            print(f"❌ Speaker left room {room_id}, clientId={client_id}")
            room["speakers"].pop(ws, None)

        if not room["microphones"] and not room["speakers"] and room["flush_task"]:
            room["flush_task"].cancel()
            del rooms[room_id]
            print(f"🧹 Room {room_id} fully cleaned")

    return ws

# App setup
app = web.Application()
app.add_routes(routes)

if __name__ == "__main__":
    print(f"🔧 FILTER_SELF_AUDIO = {FILTER_SELF_AUDIO}")
    if not FILTER_SELF_AUDIO:
        print("⚠️ This may cause a feedback loop if the same client sends and receives audio.")
        print("👉 It's recommended to set FILTER_SELF_AUDIO = True for most setups.")
    print(f"🚀 Starting Audio Proxy | Port: {PORT}")
    print(f"🎯 Channels: {CHANNELS} | Sample Rate: {SAMPLE_RATE} Hz")
    web.run_app(app, port=PORT)
