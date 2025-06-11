#pragma once

#include <mfidl.h>		 // IMFMediaSource, IMFSourceReader
#include <Mfreadwrite.h> // IMFSourceReaderCallback

#include <assert.h>

#include "../utils.h"

using namespace flutter;

#define MF_AUDIO_RENDERER_ATTRIBUTE_FLAG_ENABLE_VOICE 0x1

namespace recording
{
	class Recorder : public IMFSourceReaderCallback
	{
	public:
		static HRESULT CreateInstance(EventStreamHandler<> *recordEventHandler, Recorder **recorder);

		Recorder(EventStreamHandler<> *recordEventHandler);
		virtual ~Recorder();

		HRESULT Recorder::Start(std::string recorderId, LPCWSTR deviceId);
		HRESULT Pause();
		HRESULT Resume();
		HRESULT Stop();
		bool IsPaused();
		bool IsReady();
		HRESULT Dispose();

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID iid, void **ppv);
		STDMETHODIMP_(ULONG)
		AddRef();
		STDMETHODIMP_(ULONG)
		Release();

		// IMFSourceReaderCallback methods
		STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample *imfSample);
		STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *);
		STDMETHODIMP OnFlush(DWORD);

	private:
		HRESULT EndRecording();

		long m_LockCount;
		IMFSourceReader *m_imfReader;
		IMFMediaSource *m_imfSource;
		IMFPresentationDescriptor *m_imfDescriptor;
		bool m_imfStarted = false;
		bool m_paused = false;

		EventStreamHandler<> *m_recordEventHandler;
	};
};