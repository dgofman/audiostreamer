# server.py
import sys
import asyncio
import sounddevice as sd
from aiohttp import ClientSession

sys.path.append('..')
from settings import HOST, PORT, SAMPLE_RATE, CHANNELS, CHUNK, list_devices

WS_URL = f"http://{HOST}:{PORT}/recording"

async def main():
    loop = asyncio.get_running_loop()
    stop_event = asyncio.Event()

    async with ClientSession() as session:
        async with session.ws_connect(WS_URL) as ws:
            print("🎙️ Connected to proxy for recording")

            def callback(indata, frames, time, status):
                if status:
                    print("⚠️ Input stream warning:", status)
                try:
                    data = indata.tobytes()
                    future = asyncio.run_coroutine_threadsafe(ws.send_bytes(data), loop)
                    future.result()
                except Exception as e:
                    print("Error sending audio:", e)
                    stop_event.set()

            with sd.InputStream(
                samplerate=SAMPLE_RATE,
                blocksize=CHUNK,
                channels=CHANNELS,
                dtype='int16',
                callback=callback):
                await stop_event.wait() # run forever

if __name__ == "__main__":
    list_devices(sd, False)
    device_index = int(input("Select input device index: "))
    sd.default.device = (device_index, None)  # (input, output)
    asyncio.run(main())
