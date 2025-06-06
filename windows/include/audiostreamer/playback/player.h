#pragma once

#include <windows.h>
#include <mfidl.h>
#include <mfapi.h>
#include <mferror.h>
#include <shlwapi.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

#include <thread>
#include <mutex>
#include <deque>
#include <vector>
#include <cstdint>

#include "../utils.h"

using namespace flutter;

namespace playback
{
	enum PlayerState
	{
		START,
		STOP,
		PAUSE
	};

	class Player
	{
	public:
		static HRESULT CreateInstance(EventStreamHandler<> *stateEventHandler,
									  EventStreamHandler<> *playerEventHandler,
									  Player **player);

		Player(EventStreamHandler<> *stateEventHandler,
			   EventStreamHandler<> *playerEventHandler);
		virtual ~Player();

		HRESULT Start(std::string playerId, LPCWSTR deviceId);
		HRESULT Stop();
		HRESULT SetVolume(float volume);
		HRESULT AddChunk(const std::vector<uint8_t>& data);
		bool IsCreated();
		bool IsListening();
		HRESULT Dispose();
		void FeedAudioData(const std::vector<uint8_t> &audioData);

		// IUnknown methods (still needed for COM)
		STDMETHODIMP QueryInterface(REFIID iid, void **ppv);
		STDMETHODIMP_(ULONG)
		AddRef();
		STDMETHODIMP_(ULONG)
		Release();

	private:
		void UpdateState(PlayerState state);
		HRESULT EndPlayback();
		void PlaybackThread();
		bool IsFormatSupported(const WAVEFORMATEX *pFormat);

		long m_LockCount;
		HANDLE m_audioSamplesReadyEvent;
		UINT32 m_bytesPerFrame = 0;
		std::thread m_playbackThread;
		bool m_shutdown = false;
		bool m_listening = false;

		// WASAPI interfaces
		IAudioClient *m_audioClient = nullptr;
		IAudioRenderClient *m_renderClient = nullptr;
		UINT32 m_bufferFrameCount = 0;
		WAVEFORMATEX m_desiredFormat = {};

		// Audio data management
		std::mutex m_queueMutex;
		std::deque<std::vector<uint8_t>> m_audioDataQueue;

		EventStreamHandler<> *m_stateEventHandler;
		EventStreamHandler<> *m_playerEventHandler;
		PlayerState m_playerState = PlayerState::STOP;
	};
};