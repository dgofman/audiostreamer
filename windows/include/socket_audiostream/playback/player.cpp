#include "player.h"
#include "mediaplayer.h"
#include "../utils.h"

namespace playback
{
	HRESULT Player::CreateInstance(Player **ppPlayer)
	{
		auto pPlayer = new (std::nothrow) Player();
		if (!pPlayer)
			return E_OUTOFMEMORY;
		*ppPlayer = pPlayer;
		return S_OK;
	}

	Player::Player() : m_shutdown(true),
					   m_audioClient(nullptr),
					   m_renderClient(nullptr),
					   m_rnnoiseState(nullptr),
					   m_denoiseLevel(DenoiseLevel::NONE) {}

	Player::~Player()
	{
		Dispose();
	}

	HRESULT Player::Start(std::string playerId, LPCWSTR deviceId)
	{
		HRESULT hr = EndPlayback();
		if (FAILED(hr))
			return hr;

		hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if (FAILED(hr))
			return hr;

		IMMDeviceEnumerator *enumerator = nullptr;
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
							  __uuidof(IMMDeviceEnumerator), (void **)&enumerator);

		IMMDevice *device = nullptr;
		if (SUCCEEDED(hr))
		{
			hr = deviceId ? enumerator->GetDevice(deviceId, &device) : enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
		}

		if (SUCCEEDED(hr))
			hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&m_audioClient);

		WAVEFORMATEX *mixFormat = nullptr;
		if (SUCCEEDED(hr))
			hr = m_audioClient->GetMixFormat(&mixFormat);

		m_desiredFormat = *mixFormat;
#ifdef STEREO
		m_desiredFormat.nChannels = 2;
		m_desiredFormat.nSamplesPerSec = 48000;
#else
		m_desiredFormat.nChannels = 1;
		m_desiredFormat.nSamplesPerSec = 16000;
#endif
		m_desiredFormat.wFormatTag = WAVE_FORMAT_PCM;
		m_desiredFormat.wBitsPerSample = 16;

		// Set format to 48kHz mono for RNNoise compatibility
		if (m_denoiseLevel != DenoiseLevel::NONE)
		{
			m_desiredFormat.nSamplesPerSec = 48000;
		}
		m_desiredFormat.nBlockAlign = (m_desiredFormat.nChannels * m_desiredFormat.wBitsPerSample) / 8;
		m_desiredFormat.nAvgBytesPerSec = m_desiredFormat.nSamplesPerSec * m_desiredFormat.nBlockAlign;
		m_desiredFormat.cbSize = 0;

		REFERENCE_TIME soundBufferDuration = (REFERENCE_TIME)(REFTIMES_PER_SEC * BUFFER_SIZE_IN_SECONDS);
		hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
									   AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
									   soundBufferDuration, 0, &m_desiredFormat, NULL);
		if (FAILED(hr))
		{
			DebugPrint("Falling back to mixFormat\n");
			m_desiredFormat = *mixFormat;
			hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
										   AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
										   soundBufferDuration, 0, &m_desiredFormat, NULL);
		}

		DebugPrint("Device wFormatTag: %d, Channels: %d, nSamplesPerSec: %d, nBlockAlign: %u, nAvgBytesPerSec: %u, wBitsPerSample: %u, cbSize: %d, denoiseLevel: %d\n",
				   m_desiredFormat.wFormatTag, m_desiredFormat.nChannels, m_desiredFormat.nSamplesPerSec,
				   m_desiredFormat.nBlockAlign, m_desiredFormat.nAvgBytesPerSec, m_desiredFormat.wBitsPerSample, m_desiredFormat.cbSize, m_denoiseLevel);

		if (SUCCEEDED(hr))
			hr = m_audioClient->GetService(__uuidof(IAudioRenderClient), (void **)&m_renderClient);

		if (SUCCEEDED(hr))
			hr = m_audioClient->GetBufferSize(&m_bufferFrameCount);

		if (SUCCEEDED(hr))
		{
			m_shutdown = false;
			m_playbackThread = std::thread(&Player::PlaybackThread, this);
		}

		if (SUCCEEDED(hr))
		{
			hr = m_audioClient->Start();
		}

		CoTaskMemFree(mixFormat);
		SafeRelease(device);
		SafeRelease(enumerator);

		if (FAILED(hr))
			EndPlayback();

		return hr;
	}

	HRESULT Player::Stop() { return EndPlayback(); }
	HRESULT Player::Dispose()
	{
		// Clean up RNNoise
		if (m_rnnoiseState)
		{
			rnnoise_destroy(m_rnnoiseState);
			m_rnnoiseState = nullptr;
		}
		return EndPlayback();
	}

	HRESULT Player::SetVolume(float volume)
	{
		if (!m_audioClient)
			return E_FAIL;
		ISimpleAudioVolume *audioVolume = nullptr;
		HRESULT hr = m_audioClient->GetService(__uuidof(ISimpleAudioVolume), (void **)&audioVolume);
		if (SUCCEEDED(hr))
		{
			hr = audioVolume->SetMasterVolume(volume, NULL);
			SafeRelease(audioVolume);
		}
		return hr;
	}

	HRESULT Player::SetJitterRange(uint32_t minMs, uint32_t maxMs)
	{
		m_minJitterMs = minMs;
		m_maxJitterMs = maxMs;
		return S_OK;
	}

	HRESULT Player::AddChunk(const std::vector<uint8_t> &data)
	{
		if (m_shutdown)
			return E_FAIL;
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_jitterBuffer.insert(m_jitterBuffer.end(), data.begin(), data.end());
		return S_OK;
	}

	HRESULT Player::SetDenoise(DenoiseLevel level)
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_shutdown = true; // Signal playback thread to exit (required calling Start again)

		if (m_rnnoiseState)
		{
			rnnoise_destroy(m_rnnoiseState);
			m_rnnoiseState = nullptr;
		}

		if (level != DenoiseLevel::NONE)
		{
			m_rnnoiseState = rnnoise_create(level == DenoiseLevel::FULL ? 1 : 0);
			if (!m_rnnoiseState)
			{
				DebugPrint("ERROR: Failed to create RNNoise state for level %d\n", (int)level);
				return E_FAIL;
			}
		}

		m_denoiseLevel = level;
		return S_OK;
	}

	bool Player::IsCreated() { return m_audioClient != nullptr; }

	bool Player::IsReady() { return !m_shutdown; }

	bool Player::IsStereo()
	{
#ifdef STEREO
		return true;
#endif
		return false;
	}

	HRESULT Player::EndPlayback()
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_shutdown = true;
		if (m_playbackThread.joinable())
			m_playbackThread.join();
		if (m_audioClient)
		{
			m_audioClient->Stop();
		}
		m_jitterBuffer.clear();
		SafeRelease(m_renderClient);
		SafeRelease(m_audioClient);
		CoUninitialize();

		return S_OK;
	}

	void Player::PlaybackThread()
	{
		CoInitializeEx(NULL, COINIT_MULTITHREADED);

		// Audio format constants
		const int INPUT_FRAME_SIZE = 160; // 10ms at 16kHz

		// Calculate frame sizes in bytes
		const size_t inputFrameBytes = INPUT_FRAME_SIZE * sizeof(int16_t); // 10ms at 16kHz
		const size_t processingFrameBytes = FRAME_SIZE * sizeof(int16_t);  // 10ms at 48kHz (480 bytes)

		// Buffers for audio processing
		std::vector<float> inputFloatBuffer;
		std::vector<float> processingBuffer(FRAME_SIZE);

		while (!m_shutdown)
		{
			UINT32 padding = 0;
			if (FAILED(m_audioClient->GetCurrentPadding(&padding)))
				continue;

			UINT32 framesAvailable = m_bufferFrameCount - padding;
			if (framesAvailable == 0)
				continue;

			BYTE *buffer = nullptr;
			if (FAILED(m_renderClient->GetBuffer(framesAvailable, &buffer)))
				continue;

			UINT32 bytesToWrite = framesAvailable * m_desiredFormat.nBlockAlign;

			size_t buffered;
			{
				std::lock_guard<std::mutex> lock(m_queueMutex);
				buffered = m_jitterBuffer.size();
			}

			const size_t bytesPerMs = m_desiredFormat.nAvgBytesPerSec / 1000;
			const size_t minBytes = m_minJitterMs * bytesPerMs;
			const size_t maxBytes = m_maxJitterMs * bytesPerMs;

			if (buffered < minBytes)
			{
				memset(buffer, 0, bytesToWrite);
				m_renderClient->ReleaseBuffer(framesAvailable, 0);
				continue;
			}

			if (buffered > maxBytes)
			{
				size_t drop = buffered - maxBytes;
				std::lock_guard<std::mutex> lock(m_queueMutex);
				m_jitterBuffer.erase(m_jitterBuffer.begin(), m_jitterBuffer.begin() + drop);
				buffered -= drop;
			}

			{
				std::lock_guard<std::mutex> lock(m_queueMutex);
				size_t toCopy = min(bytesToWrite, m_jitterBuffer.size());

				if (m_denoiseLevel == DenoiseLevel::NONE)
				{
					std::copy_n(m_jitterBuffer.begin(), toCopy, buffer);
					m_jitterBuffer.erase(m_jitterBuffer.begin(), m_jitterBuffer.begin() + toCopy);
				}
				else
				{
					size_t available = m_jitterBuffer.size();
					size_t framesAvailableToProcess = available / inputFrameBytes;
					size_t framesWritable = bytesToWrite / processingFrameBytes;
					size_t framesToProcess = min(framesAvailableToProcess, framesWritable);

					if (framesToProcess == 0 || !m_rnnoiseState)
					{
						memset(buffer, 0, bytesToWrite);
						m_renderClient->ReleaseBuffer(framesAvailable, 0);
						continue;
					}

					size_t processed = 0;
					for (size_t f = 0; f < framesToProcess; ++f)
					{
						const int16_t *input16k = reinterpret_cast<const int16_t *>(&m_jitterBuffer[processed]);
						// RNNoise expects integer-range values (-32768 to 32767), not normalized floats
						inputFloatBuffer.resize(INPUT_FRAME_SIZE);
						for (int i = 0; i < INPUT_FRAME_SIZE; i++)
						{
							inputFloatBuffer[i] = static_cast<float>(input16k[i]); // No division
						}

						// Upsample from 16kHz to 48kHz (1:3) with linear interpolation
						for (int i = 0; i < INPUT_FRAME_SIZE - 1; i++)
						{
							float s0 = inputFloatBuffer[i];
							float s1 = inputFloatBuffer[i + 1];
							processingBuffer[i * 3] = s0;
							processingBuffer[i * 3 + 1] = (2.f * s0 + s1) / 3.f;
							processingBuffer[i * 3 + 2] = (s0 + 2.f * s1) / 3.f;
						}
						processingBuffer[(INPUT_FRAME_SIZE - 1) * 3] = inputFloatBuffer[INPUT_FRAME_SIZE - 1];

						// Process frame through RNNoise
						rnnoise_process_frame(m_rnnoiseState, processingBuffer.data(), processingBuffer.data());

						// Convert processed audio to output format and copy to device buffer
						int16_t *outputBuffer = reinterpret_cast<int16_t *>(buffer) + (f * FRAME_SIZE);
						for (int i = 0; i < FRAME_SIZE; i++)
						{
							outputBuffer[i] = static_cast<int16_t>(
								max(-32768.0f, min(32767.0f, processingBuffer[i])));
						}

						processed += inputFrameBytes;
					}

					m_jitterBuffer.erase(m_jitterBuffer.begin(),
										 m_jitterBuffer.begin() + framesToProcess * inputFrameBytes);
				}
			}

			m_renderClient->ReleaseBuffer(framesAvailable, 0);
		}
		CoUninitialize();
	}
};