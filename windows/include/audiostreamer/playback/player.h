#pragma once

#include <Audioclient.h> // m_audioClient

#include <thread>
#include <mutex>
#include <deque>
#include <vector>

#include "../utils.h"

namespace playback
{
	class Player
	{
	public:
		static HRESULT CreateInstance(Player **player);

		Player();
		virtual ~Player();

		HRESULT Start(std::string playerId, LPCWSTR deviceId);
		HRESULT Stop();
		HRESULT SetVolume(float volume);
		HRESULT AddChunk(const std::vector<uint8_t> &data);
		bool IsCreated();
		bool IsReady();
		HRESULT Dispose();
		void FeedAudioData(const std::vector<uint8_t> &audioData);

	private:
		HRESULT EndPlayback();
		void PlaybackThread();
		bool IsFormatSupported(const WAVEFORMATEX *pFormat);

		HANDLE m_audioSamplesReadyEvent;
		std::thread m_playbackThread;
		bool m_shutdown = false;
		bool m_isready = false;

		// WASAPI interfaces
		IAudioClient *m_audioClient = nullptr;
		IAudioRenderClient *m_renderClient = nullptr;
		UINT32 m_bufferFrameCount = 0;
		WAVEFORMATEX m_desiredFormat = {};

		// Audio data management
		std::mutex m_queueMutex;
		std::deque<std::vector<uint8_t>> m_audioDataQueue;
	};
};