#pragma once

#include <Audioclient.h> // m_audioClient
#include <thread>		 // std::thread
#include <mutex>		 // std::mutex
#include <vector>		 // std::vector

#include "denoise_lite.h"

#define BUFFER_SIZE_IN_SECONDS 0.1f
#define REFTIMES_PER_SEC 10000000 // hundred nanoseconds

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
		HRESULT SetJitterRange(uint32_t minMs, uint32_t maxMs);
		bool IsCreated();
		bool IsReady();
		bool IsStereo();
		void SetDenoise(bool val);
		HRESULT Dispose();

	private:
		HRESULT EndPlayback();
		void PlaybackThread();

		IAudioClient *m_audioClient;
		IAudioRenderClient *m_renderClient;
		std::atomic<bool> m_isready;
		std::atomic<bool> m_shutdown;

		WAVEFORMATEX m_desiredFormat;
		UINT32 m_bufferFrameCount;
		std::thread m_playbackThread;
		std::mutex m_queueMutex;

		// Unified jitter
		std::vector<uint8_t> m_jitterBuffer;
		uint32_t m_minJitterMs = 200;
		uint32_t m_maxJitterMs = 800;
		uint32_t m_lastLoggedJitterMs = 0;

		std::atomic<bool> m_denoise;
		denoise_lite::DenoiseLite m_noiseSuppressor;
	};
}
