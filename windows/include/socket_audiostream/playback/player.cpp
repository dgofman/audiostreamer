#include <algorithm>					   // std::min
#include "player.h"
#include "mediaplayer.h"

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

	Player::Player() : m_audioClient(nullptr),
					   m_renderClient(nullptr),
					   m_isready(false),
					   m_shutdown(false) {}

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

		DebugPrint("Device wFormatTag: %d, Channels: %d, nSamplesPerSec: %d, nBlockAlign: %u, nAvgBytesPerSec: %u, wBitsPerSample: %u, cbSize: %d\n",
				   m_desiredFormat.wFormatTag, m_desiredFormat.nChannels, m_desiredFormat.nSamplesPerSec,
				   m_desiredFormat.nBlockAlign, m_desiredFormat.nAvgBytesPerSec, m_desiredFormat.wBitsPerSample, m_desiredFormat.cbSize);

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
			m_isready = true;
		}

		CoTaskMemFree(mixFormat);
		SafeRelease(device);
		SafeRelease(enumerator);
		if (FAILED(hr))
			EndPlayback();
		return hr;
	}

	HRESULT Player::Stop() { return EndPlayback(); }
	HRESULT Player::Dispose() { return EndPlayback(); }

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
		if (!m_isready)
			return E_FAIL;
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_jitterBuffer.insert(m_jitterBuffer.end(), data.begin(), data.end());
		return S_OK;
	}

	bool Player::IsCreated() { return m_audioClient != nullptr; }
	bool Player::IsReady() { return m_isready; }

	HRESULT Player::EndPlayback()
	{
		m_shutdown = true;
		if (m_playbackThread.joinable())
			m_playbackThread.join();
		if (m_audioClient)
		{
			m_audioClient->Stop();
			m_isready = false;
		}
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_jitterBuffer.clear();
		SafeRelease(m_renderClient);
		SafeRelease(m_audioClient);
		CoUninitialize();
		return S_OK;
	}

	void Player::PlaybackThread()
	{
		CoInitializeEx(NULL, COINIT_MULTITHREADED);
		const size_t bytesPerMs = m_desiredFormat.nAvgBytesPerSec / 1000;

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

			size_t minBytes = m_minJitterMs * bytesPerMs;
			size_t maxBytes = m_maxJitterMs * bytesPerMs;

			if (buffered < minBytes)
			{
				memset(buffer, 0, bytesToWrite);
				m_renderClient->ReleaseBuffer(framesAvailable, 0);
				continue;
			}

			if (buffered > maxBytes)
			{
				size_t drop = buffered - maxBytes;
				{
					std::lock_guard<std::mutex> lock(m_queueMutex);
					m_jitterBuffer.erase(m_jitterBuffer.begin(), m_jitterBuffer.begin() + drop);
				}
				buffered -= drop;
			}

			size_t copied = 0;
			{
				std::lock_guard<std::mutex> lock(m_queueMutex);
				size_t toCopy = std::min<size_t>(bytesToWrite, m_jitterBuffer.size());
				std::copy_n(m_jitterBuffer.begin(), toCopy, buffer);
				m_jitterBuffer.erase(m_jitterBuffer.begin(), m_jitterBuffer.begin() + toCopy);
				copied = toCopy;
			}

			m_renderClient->ReleaseBuffer(framesAvailable, 0);

			uint32_t msBuffered = (uint32_t)(buffered * 1000 / m_desiredFormat.nAvgBytesPerSec);
			if (abs((int)msBuffered - (int)m_lastLoggedJitterMs) > 50)
			{
				DebugPrint("Jitter: %ums [min:%ums max:%ums] buffered:%zuB frames:%u\n",
						   msBuffered, m_minJitterMs, m_maxJitterMs, buffered, framesAvailable);
				m_lastLoggedJitterMs = msBuffered;
			}
		}
		CoUninitialize();
	}
};
