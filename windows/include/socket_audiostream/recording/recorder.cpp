#include "recorder.h"
#include "mediarecorder.h"

namespace recording
{
	STDMETHODIMP Recorder::QueryInterface(REFIID iid, void **ppv)
	{
		if (!ppv)
			return E_POINTER;
		*ppv = nullptr;
		if (iid == __uuidof(IUnknown))
		{
			*ppv = static_cast<IUnknown *>(static_cast<IMFSourceReaderCallback *>(this));
		}
		else if (iid == __uuidof(IMFSourceReaderCallback))
		{
			*ppv = static_cast<IMFSourceReaderCallback *>(this);
		}
		else
		{
			return E_NOINTERFACE;
		}
		AddRef();
		return S_OK;
	}

	STDMETHODIMP_(ULONG)
	Recorder::AddRef()
	{
		return InterlockedIncrement(&m_LockCount);
	}

	STDMETHODIMP_(ULONG)
	Recorder::Release()
	{
		ULONG uCount = InterlockedDecrement(&m_LockCount);
		if (uCount == 0)
		{
			delete this;
		}
		return uCount;
	}

	STDMETHODIMP Recorder::OnEvent(DWORD, IMFMediaEvent *)
	{
		return S_OK;
	}

	STDMETHODIMP Recorder::OnFlush(DWORD)
	{
		return S_OK;
	}

	HRESULT Recorder::OnReadSample(
		HRESULT hrStatus,
		DWORD dwStreamIndex,
		DWORD dwStreamFlags,
		LONGLONG llTimestamp,
		IMFSample *imfSample)
	{
		HRESULT hr = S_OK;

		if (SUCCEEDED(hrStatus))
		{
			if (imfSample)
			{
				hr = imfSample->SetSampleTime(llTimestamp);

				if (SUCCEEDED(hr))
				{
					IMFMediaBuffer *pBuffer = NULL;
					hr = imfSample->ConvertToContiguousBuffer(&pBuffer);

					if (SUCCEEDED(hr))
					{
						BYTE *pChunk = NULL;
						DWORD size = 0;
						hr = pBuffer->Lock(&pChunk, NULL, &size);

						if (SUCCEEDED(hr))
						{
							// Send data to stream
							if (m_recordEventHandler)
							{
								std::vector<uint8_t> bytes(pChunk, pChunk + size);
								MediaRecorder::CallbackHandler([this, bytes]() -> void
															   { m_recordEventHandler->Success(std::make_unique<flutter::EncodableValue>(bytes)); });
							}
							pBuffer->Unlock();
						}
						SafeRelease(pBuffer);
					}
				}
			}

			if (SUCCEEDED(hr))
			{
				hr = m_imfReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
											 0, NULL, NULL, NULL, NULL);
			}
		}
		else
		{
			auto errorText = std::system_category().message(hrStatus);
			printf("Record: Error when reading sample (0x%X)\n%s\n", hrStatus, errorText.c_str());
			Stop();
		}

		return hr;
	}

	HRESULT Recorder::CreateInstance(EventStreamHandler<> *recordEventHandler, Recorder **ppRecorder)
	{
		auto pRecorder = new (std::nothrow) Recorder(recordEventHandler);
		if (pRecorder == NULL)
		{
			return E_OUTOFMEMORY;
		}
		*ppRecorder = pRecorder;

		return S_OK;
	}

	Recorder::Recorder(EventStreamHandler<> *recordEventHandler)
		: m_LockCount(1),
		  m_imfSource(NULL),
		  m_imfReader(NULL),
		  m_imfDescriptor(NULL),
		  m_recordEventHandler(recordEventHandler)
	{
	}

	Recorder::~Recorder()
	{
		Dispose();
	}

	HRESULT Recorder::Start(std::string recorderId, LPCWSTR deviceId)
	{
		HRESULT hr = EndRecording();

		if (SUCCEEDED(hr))
		{
			if (!m_imfStarted)
			{
				hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
			}
			if (SUCCEEDED(hr))
			{
				m_imfStarted = true;
			}
		}

		if (SUCCEEDED(hr))
		{
			IMFAttributes *pAttributes = NULL;
			hr = MFCreateAttributes(&pAttributes, 2);
			// Enable speech processing mode for AEC, NS, AGC
			pAttributes->SetUINT32(MF_AUDIO_RENDERER_ATTRIBUTE_FLAGS, MF_AUDIO_RENDERER_ATTRIBUTE_FLAG_ENABLE_VOICE);

			// Set the device type to audio.
			if (SUCCEEDED(hr))
			{
				hr = pAttributes->SetGUID(
					MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
					MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
			}

			// Set the endpoint ID.
			if (SUCCEEDED(hr) && deviceId)
			{
				hr = pAttributes->SetString(
					MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID,
					deviceId);
			}

			// Create the source
			if (SUCCEEDED(hr))
			{
				hr = MFCreateDeviceSource(pAttributes, &m_imfSource);
			}
			// Create presentation descriptor to handle Resume action
			if (SUCCEEDED(hr))
			{
				hr = m_imfSource->CreatePresentationDescriptor(&m_imfDescriptor);
			}

			SafeRelease(pAttributes);
		}
		if (SUCCEEDED(hr))
		{
			hr = S_OK;
			IMFAttributes *pAttributes = NULL;
			IMFMediaType *pMediaTypeIn = NULL;

			hr = MFCreateAttributes(&pAttributes, 1);
			if (SUCCEEDED(hr))
			{
				hr = pAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this);
			}
			if (SUCCEEDED(hr))
			{
				hr = MFCreateSourceReaderFromMediaSource(m_imfSource, pAttributes, &m_imfReader);
			}
			if (SUCCEEDED(hr))
			{
				IMFMediaType **ppMediaType = &pMediaTypeIn;
				hr = hr = S_OK;

				IMFMediaType *pMediaType = NULL;

				hr = MFCreateMediaType(&pMediaType);

				if (SUCCEEDED(hr))
				{
					hr = pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
				}
				if (SUCCEEDED(hr))
				{
					hr = pMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
				}
				if (SUCCEEDED(hr))
				{
					hr = pMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
				}
				if (SUCCEEDED(hr))
				{
					hr = pMediaType->SetUINT32(MF_MT_AVG_BITRATE, 128000);
				}
				if (SUCCEEDED(hr))
				{
#ifdef STEREO
						hr = pMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
#else
						hr = pMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 16000);
#endif
				}
				if (SUCCEEDED(hr))
				{
#ifdef STEREO
					hr = pMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
#else
					hr = pMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
#endif
				}
				UINT32 samples, channels;
				pMediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samples);
				pMediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
				DebugPrint("MF_MT_AUDIO_SAMPLES_PER_SECOND: %u, MF_MT_AUDIO_NUM_CHANNELS:%u\n", samples, channels);
				if (SUCCEEDED(hr))
				{
					*ppMediaType = pMediaType;
					(*ppMediaType)->AddRef();
				}

				SafeRelease(pMediaType);
			}
			if (SUCCEEDED(hr))
			{
				hr = m_imfReader->SetCurrentMediaType(0, NULL, pMediaTypeIn);
			}

			SafeRelease(pMediaTypeIn);
			SafeRelease(pAttributes);
		}

		if (SUCCEEDED(hr))
		{
			hr = m_imfReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
										 0,
										 NULL, NULL, NULL, NULL);
		}

		if (!SUCCEEDED(hr))
		{
			EndRecording();
		}

		return hr;
	}

	HRESULT Recorder::Pause()
	{
		HRESULT hr = S_OK;
		if (m_imfSource)
		{
			hr = m_imfSource->Pause();
		}

		if (SUCCEEDED(hr))
		{
			m_paused = true;
		}

		return S_OK;
	}

	HRESULT Recorder::Resume()
	{
		HRESULT hr = S_OK;

		if (m_imfSource)
		{
			PROPVARIANT var;
			PropVariantInit(&var);
			var.vt = VT_EMPTY;

			hr = m_imfSource->Start(m_imfDescriptor, NULL, &var);
		}

		if (SUCCEEDED(hr))
		{
			m_paused = false;
		}

		return hr;
	}

	HRESULT Recorder::Stop()
	{
		return EndRecording();
	}

	bool Recorder::IsPaused()
	{
		return m_paused;
	}

	bool Recorder::IsReady()
	{
		return m_imfStarted;
	}

	HRESULT Recorder::EndRecording()
	{
		HRESULT hr = S_OK;
		SafeRelease(m_imfReader);

		if (m_imfSource)
		{
			hr = m_imfSource->Stop();

			if (SUCCEEDED(hr))
			{
				hr = m_imfSource->Shutdown();
			}
		}

		if (m_imfStarted)
		{
			hr = MFShutdown();
			if (SUCCEEDED(hr))
			{
				m_imfStarted = false;
			}
		}

		SafeRelease(m_imfSource);
		SafeRelease(m_imfDescriptor);
		return hr;
	}

	HRESULT Recorder::Dispose()
	{
		HRESULT hr = EndRecording();
		m_recordEventHandler = nullptr;
		return hr;
	}
};