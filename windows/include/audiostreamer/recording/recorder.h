#pragma once

#include <windows.h>
#include <mfidl.h>
#include <mfapi.h>
#include <mferror.h>
#include <shlwapi.h>
#include <Mfreadwrite.h>

#include <assert.h>

#include "../utils.h"

using namespace flutter;

namespace recording
{
	enum RecordState
	{
		RECORD,
		STOP,
		PAUSE
	};

	class Recorder : public IMFSourceReaderCallback
	{
	public:
		static HRESULT CreateInstance(EventStreamHandler<> *stateEventHandler, EventStreamHandler<> *recordEventHandler, Recorder **recorder);

		Recorder(EventStreamHandler<> *stateEventHandler, EventStreamHandler<> *recordEventHandler);
		virtual ~Recorder();

		HRESULT Recorder::Start(std::string recorderId, LPCWSTR deviceId);
		HRESULT Pause();
		HRESULT Resume();
		HRESULT Stop();
		bool IsPaused();
		bool IsRecording();
		HRESULT Dispose();

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID iid, void **ppv);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();

		// IMFSourceReaderCallback methods
		STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample *imfSample);
		STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *);
		STDMETHODIMP OnFlush(DWORD);

	private:
		void UpdateState(RecordState state);
		HRESULT EndRecording();

		long m_LockCount;
		IMFSourceReader *m_imfReader;
		IMFMediaSource *m_imfSource;
		IMFPresentationDescriptor *m_imfDescriptor;
		bool m_imfStarted = false;

		EventStreamHandler<> *m_stateEventHandler;
		EventStreamHandler<> *m_recordEventHandler;
		RecordState m_recordState = RecordState::STOP;
	};
};