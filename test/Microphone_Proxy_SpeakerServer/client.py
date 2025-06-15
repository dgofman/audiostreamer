import sys
import asyncio
import sounddevice as sd
from aiohttp import ClientSession

sys.path.append('..')
from utils import HOST, PORT, SAMPLE_RATE, CHANNELS, CHUNK, PATH, IN_ROLE, \
                    list_devices, get_local_ip

async def main(room_id, client_id):
    loop = asyncio.get_running_loop()
    stop_event = asyncio.Event()

    ws_url = f'ws://{HOST}:{PORT}/{PATH}?roomId={room_id}&clientId={client_id}&role={IN_ROLE}'

    async with ClientSession() as session:
        async with session.ws_connect(ws_url) as ws:
            print(f"🎙️ Microphone streaming started in room '{room_id}' as clientId '{client_id}'")

            def callback(indata, frames, time, status):
                if status:
                    print('⚠️ Input stream warning:', status)
                try:
                    data = indata.tobytes()
                    future = asyncio.run_coroutine_threadsafe(ws.send_bytes(data), loop)
                    future.result()
                except Exception as e:
                    print('❌ Error sending audio:', e)
                    stop_event.set()

            with sd.InputStream(
                samplerate=SAMPLE_RATE,
                blocksize=CHUNK,
                channels=CHANNELS,
                dtype='int16',
                callback=callback):
                await stop_event.wait()  # run until stop_event is triggered

if __name__ == '__main__':
    list_devices(sd, False)
    device_index = int(input('Select input device index: '))
    sd.default.device = (device_index, None)

    room_id = input('Enter room ID: ')
    client_id = input('Enter client ID (or leave blank to use IP): ') or get_local_ip()

    asyncio.run(main(room_id, client_id))
