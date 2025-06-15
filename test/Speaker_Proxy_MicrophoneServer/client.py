import sys
import asyncio
import sounddevice as sd
import numpy as np
from aiohttp import ClientSession, WSMsgType
from threading import Lock

sys.path.append('..')
from utils import HOST, PORT, SAMPLE_RATE, CHANNELS, CHUNK, BYTE_DEPTH, PATH, OUT_ROLE, \
                    list_devices, get_local_ip

class AudioBuffer:
    def __init__(self):
        self.buffer = bytearray()
        self.lock = Lock()

    def append(self, data):
        with self.lock:
            self.buffer.extend(data)

    def consume(self, size):
        with self.lock:
            if len(self.buffer) >= size:
                out = self.buffer[:size]
                del self.buffer[:size]
                return out
            return None

async def audio_buffer(room_id, client_id):
    buffer = AudioBuffer()
    ws_url = f'ws://{HOST}:{PORT}/{PATH}?roomId={room_id}&clientId={client_id}&role={OUT_ROLE}'

    def callback(outdata, frames, time, status):
        chunk = buffer.consume(CHUNK * BYTE_DEPTH * CHANNELS)
        if chunk:
            outdata[:] = np.frombuffer(chunk, dtype=np.int16).reshape(-1, CHANNELS)
        else:
            outdata[:] = np.zeros((CHUNK, CHANNELS), dtype=np.int16)

    async with ClientSession() as session:
        async with session.ws_connect(ws_url) as ws:
            print(f"🔊 Speaker connected to room '{room_id}' with clientId '{client_id}'")

            with sd.OutputStream(
                samplerate=SAMPLE_RATE,
                blocksize=CHUNK,
                channels=CHANNELS,
                dtype='int16',
                callback=callback
            ):
                async for msg in ws:
                    if msg.type == WSMsgType.BINARY:
                        buffer.append(msg.data)

if __name__ == '__main__':
    list_devices(sd, True)
    device_index = int(input('Select output device index: '))
    sd.default.device = (None, device_index)

    room_id = input('Enter room ID: ')
    client_id = input('Enter client ID (or leave blank to use IP): ') or get_local_ip()

    asyncio.run(audio_buffer(room_id, client_id))