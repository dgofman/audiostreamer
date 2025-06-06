# client.py
import sys
import asyncio
import sounddevice as sd
import numpy as np
from aiohttp import ClientSession, WSMsgType
from threading import Lock

sys.path.append('..')
from settings import SAMPLE_RATE, CHANNELS, CHUNK, list_devices

BYTE_DEPTH = 2
WS_URL = "http://127.0.0.1:9000/listening"

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

async def audioBuffer():
    buffer = AudioBuffer()

    def callback(outdata, frames, time, status):
        chunk = buffer.consume(CHUNK * BYTE_DEPTH * CHANNELS)
        if chunk:
            outdata[:] = np.frombuffer(chunk, dtype=np.int16).reshape(-1, CHANNELS)
        else:
            outdata[:] = np.zeros((CHUNK, CHANNELS), dtype=np.int16)

    async with ClientSession() as session:
        async with session.ws_connect(WS_URL) as ws:
            print("🔊 Speaker(s) streaming started...")

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

if __name__ == "__main__":
    list_devices(sd, True)
    device_index = int(input("Select output device index: "))
    sd.default.device = (None, device_index)  # (input, output)
    asyncio.run(audioBuffer())
