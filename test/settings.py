STEREO = True
if STEREO:
    # 2 channels × 16 bits × 48000 samples/sec = 1536 Kbps ≈ 192 KB/sec
    SAMPLE_RATE = 48000
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
    print(f"\n🔎 Listing devices for host API: {hostapi_name}")
    
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


