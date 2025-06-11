import argparse

# Parse command-line arguments
parser = argparse.ArgumentParser(description="AudioStream")
parser.add_argument('--stereo', action='store_true', help='Enable stereo mode (default: mono)')
parser.add_argument('--host', default='127.0.0.1', help='Set host (default: 127.0.0.1)')
parser.add_argument('--port', type=int, default=50000, help='Set port (default: 50000)')
args = parser.parse_args()

HOST = args.host
PORT = args.port

# Set STEREO based on CLI flag
STEREO = args.stereo

if STEREO:
    # 2 channels × 16 bits × 44,100 samples/sec = 1,411.2 kbps ≈ 176.4 KB/sec (44,100 Hz: CD quality)
    SAMPLE_RATE = 44100
    CHANNELS = 2
else:
    # 1 channel × 16 bits × 16000 samples/sec = 256 Kbps ≈ 32 KB/sec
    SAMPLE_RATE = 16000
    CHANNELS = 1

CHUNK = 1024

'''
hostapi_name: 
0=MME
1=Windows DirectSound
2=Windows WASAPI
3=Windows WDM-KS
'''
def list_devices(sd, isOutput,  hostapi_name='MME'):
    print(f"\n🔎 Audio Devices [{hostapi_name}] — Channels: {'Mono' if CHANNELS == 1 else 'Stereo'}, Host: {HOST}:{PORT}")

    
    hostapis = sd.query_hostapis()
    hostapi_index = next((i for i, api in enumerate(hostapis) if api['name'] == hostapi_name), None)

    if hostapi_index is None:
        print(f"❌ Host API '{hostapi_name}' not found.")
        return

    if isOutput:
        print("\n🔊 Output Devices:")
        for idx, device in enumerate(sd.query_devices()):
            if device['hostapi'] == hostapi_index and device['max_output_channels'] > 0:
                print(f"{idx}: {device['name']} (Channels: {device['max_output_channels']})")
    else:
        print("\n🎙️ Input Devices:")
        for idx, device in enumerate(sd.query_devices()):
            if device['hostapi'] == hostapi_index and device['max_input_channels'] > 0:
                print(f"{idx}: {device['name']} (Channels: {device['max_input_channels']})")


