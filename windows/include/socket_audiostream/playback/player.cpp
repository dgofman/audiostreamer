
#define NOMINMAX
#include <algorithm>					   // std::min
#include "player.h"
#include "mediaplayer.h"

namespace playback
{
	HRESULT Player::CreateInstance(Player **ppPlayer)
	{
		auto pPlayer = new (std::nothrow) Player();
		if (pPlayer == NULL)
		{
			return E_OUTOFMEMORY;
		}
		*ppPlayer = pPlayer;
		return S_OK;
	}

	Player::Player() : m_audioClient(nullptr),
					   m_renderClient(nullptr),
					   m_isready(false),
					   m_shutdown(false)
	{
		m_audioSamplesReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	}

	Player::~Player()
	{
		Dispose();
		CloseHandle(m_audioSamplesReadyEvent);
	}

	bool Player::IsFormatSupported(const WAVEFORMATEX *pFormat)
	{
		if (!m_audioClient)
			return false;

		WAVEFORMATEX *pClosestMatch = nullptr;
		HRESULT hr = m_audioClient->IsFormatSupported(
			AUDCLNT_SHAREMODE_SHARED,
			pFormat,
			&pClosestMatch);

		if (pClosestMatch)
		{
			CoTaskMemFree(pClosestMatch);
		}

		return (hr == S_OK);
	}

	HRESULT Player::Start(std::string playerId, LPCWSTR deviceId)
	{
		HRESULT hr = S_OK;

		// Stop if already playing
		hr = EndPlayback();
		if (FAILED(hr))
		{
			return hr;
		}

		// Initialize COM for this thread
		hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if (FAILED(hr))
		{
			return hr;
		}

		// Create device enumerator
		IMMDeviceEnumerator *enumerator = nullptr;
		hr = CoCreateInstance(
			__uuidof(MMDeviceEnumerator),
			NULL,
			CLSCTX_ALL,
			__uuidof(IMMDeviceEnumerator),
			(void **)&enumerator);

		IMMDevice *device = nullptr;
		if (SUCCEEDED(hr))
		{
			if (deviceId)
			{
				hr = enumerator->GetDevice(deviceId, &device);
			}
			else
			{
				hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
			}
		}

		// Activate audio client
		if (SUCCEEDED(hr))
		{
			hr = device->Activate(
				__uuidof(IAudioClient),
				CLSCTX_ALL,
				NULL,
				(void **)&m_audioClient);
		}

		// Get mix format and set to desired format
		WAVEFORMATEX *mixFormat = nullptr;
		if (SUCCEEDED(hr))
		{
			hr = m_audioClient->GetMixFormat(&mixFormat);
		}

		if (SUCCEEDED(hr))
		{
			// Match recorder format: 16kHz, mono, 16-bit
			m_desiredFormat.wFormatTag = WAVE_FORMAT_PCM;
			m_desiredFormat.nChannels = mixFormat->nChannels;
			m_desiredFormat.nSamplesPerSec = mixFormat->nSamplesPerSec;
			m_desiredFormat.wBitsPerSample = 16;
			m_desiredFormat.nBlockAlign = m_desiredFormat.nChannels * (m_desiredFormat.wBitsPerSample / 8);
			m_desiredFormat.nAvgBytesPerSec = m_desiredFormat.nSamplesPerSec * m_desiredFormat.nBlockAlign;
			m_desiredFormat.cbSize = 0;
		}

		// Check if desired format is supported
		if (!IsFormatSupported(&m_desiredFormat))
		{
			// Use mix format instead
			m_desiredFormat = *mixFormat;
		}

		// Initialize with desired format
		hr = m_audioClient->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			0,
			0,
			&m_desiredFormat,
			NULL);

		// Set event handle
		if (SUCCEEDED(hr))
		{
			hr = m_audioClient->SetEventHandle(m_audioSamplesReadyEvent);
		}

		// Get render client
		if (SUCCEEDED(hr))
		{
			hr = m_audioClient->GetService(
				__uuidof(IAudioRenderClient),
				(void **)&m_renderClient);
		}

		// Get buffer size
		if (SUCCEEDED(hr))
		{
			hr = m_audioClient->GetBufferSize(&m_bufferFrameCount);
		}

		// Start playback thread
		if (SUCCEEDED(hr))
		{
			m_shutdown = false;
			m_playbackThread = std::thread(&Player::PlaybackThread, this);
		}

		// Start audio client
		if (SUCCEEDED(hr))
		{
			hr = m_audioClient->Start();
			m_isready = true;
		}

		// Cleanup
		CoTaskMemFree(mixFormat);
		SafeRelease(device);
		SafeRelease(enumerator);

		if (FAILED(hr))
		{
			EndPlayback();
		}

		return hr;
	}

	HRESULT Player::Stop()
	{
		return EndPlayback();
	}

	HRESULT Player::SetVolume(float volume)
	{
		if (!m_audioClient)
			return E_FAIL;

		ISimpleAudioVolume *audioVolume = nullptr;
		HRESULT hr = m_audioClient->GetService(
			__uuidof(ISimpleAudioVolume),
			(void **)&audioVolume);

		if (SUCCEEDED(hr))
		{
			hr = audioVolume->SetMasterVolume(volume, NULL);
			SafeRelease(audioVolume);
		}
		return hr;
	}

	HRESULT Player::AddChunk(const std::vector<uint8_t> &data)
	{
		if (!m_isready)
			return E_FAIL;
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			m_audioDataQueue.push_back(data);
		}

		SetEvent(m_audioSamplesReadyEvent); // Trigger playback
		return S_OK;
	}

	bool Player::IsCreated()
	{
		return m_audioClient != nullptr;
	}

	bool Player::IsReady()
	{
		return m_isready;
	}

	HRESULT Player::EndPlayback()
	{
		m_shutdown = true;
		SetEvent(m_audioSamplesReadyEvent);

		if (m_playbackThread.joinable())
		{
			m_playbackThread.join();
		}

		if (m_audioClient)
		{
			m_audioClient->Stop();
			m_isready = false;
		}

		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			m_audioDataQueue.clear();
		}

		SafeRelease(m_renderClient);
		SafeRelease(m_audioClient);
		CoUninitialize();

		return S_OK;
	}

	HRESULT Player::Dispose()
	{
		return EndPlayback();
	}

	void Player::FeedAudioData(const std::vector<uint8_t> &audioData)
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_audioDataQueue.push_back(audioData);
		SetEvent(m_audioSamplesReadyEvent);
	}

	void Player::PlaybackThread()
	{
		HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if (FAILED(hr))
			return;

		HANDLE waitArray[] = {m_audioSamplesReadyEvent};

		while (!m_shutdown)
		{
			DWORD waitResult = WaitForSingleObject(m_audioSamplesReadyEvent, 50); // Wake max every 50ms
			if (waitResult != WAIT_OBJECT_0)
			{
				continue; // timeout or failure, re-check shutdown flag
			}

			UINT32 padding = 0;
			if (FAILED(m_audioClient->GetCurrentPadding(&padding)))
			{
				Sleep(1); // avoid tight retry loop
				continue;
			}

			UINT32 framesAvailable = m_bufferFrameCount - padding;
			if (framesAvailable == 0)
			{
				Sleep(1); // nothing to do
				continue;
			}

			BYTE *buffer = nullptr;
			if (FAILED(m_renderClient->GetBuffer(framesAvailable, &buffer)))
			{
				Sleep(1);
				continue;
			}

			UINT32 framesWritten = 0;

			while (framesWritten < framesAvailable && !m_shutdown)
			{
				std::vector<uint8_t> audioData;
				{
					std::lock_guard<std::mutex> lock(m_queueMutex);
					if (!m_audioDataQueue.empty())
					{
						audioData = std::move(m_audioDataQueue.front());
						m_audioDataQueue.pop_front();
					}
				}

				if (audioData.empty())
				{
					size_t silenceSize = (framesAvailable - framesWritten) * m_desiredFormat.nBlockAlign;
					memset(buffer + framesWritten * m_desiredFormat.nBlockAlign, 0, silenceSize);
					framesWritten = framesAvailable;
					break;
				}

				size_t bytesAvailable = (framesAvailable - framesWritten) * m_desiredFormat.nBlockAlign;
				size_t bytesToWrite = std::min(audioData.size(), bytesAvailable);

				memcpy(buffer + framesWritten * m_desiredFormat.nBlockAlign,
					   audioData.data(), bytesToWrite);

				framesWritten += static_cast<UINT32>(bytesToWrite / m_desiredFormat.nBlockAlign);

				if (bytesToWrite < audioData.size())
				{
					std::vector<uint8_t> remaining(audioData.begin() + bytesToWrite, audioData.end());
					std::lock_guard<std::mutex> lock(m_queueMutex);
					m_audioDataQueue.push_front(std::move(remaining));
				}
			}

			m_renderClient->ReleaseBuffer(framesWritten, 0);
		}

		CoUninitialize();
	}
};