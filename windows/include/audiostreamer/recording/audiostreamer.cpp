
#include "audiostreamer.h"
#include <mfreadwrite.h>
#include <Mferror.h>
#include <deque>

using namespace flutter;

constexpr uint32_t kCreate = HashMethodName("create");
constexpr uint32_t kHasPermission = HashMethodName("hasPermission");
constexpr uint32_t kStart = HashMethodName("start");
constexpr uint32_t kStop = HashMethodName("stop");
constexpr uint32_t kPause = HashMethodName("pause");
constexpr uint32_t kResume = HashMethodName("resume");
constexpr uint32_t kIsPaused = HashMethodName("isPaused");
constexpr uint32_t kIsRecording = HashMethodName("isRecording");
constexpr uint32_t kDispose = HashMethodName("dispose");
constexpr uint32_t kListDevices = HashMethodName("listDevices");

namespace
{
	int window_proc_delegate;
	std::function<HWND(void)> root_window_getter;
	std::deque<std::function<void()>> delegate_callbacks;
	std::map<std::string, std::unique_ptr<recording::Recorder>> m_recorders{};
	BinaryMessenger *binary_messenger = nullptr;
}

namespace recording
{
	AudioStreamer::AudioStreamer(PluginRegistrarWindows* registrar)
    : registrar_(registrar) {}

	AudioStreamer::~AudioStreamer()
	{
		for (const auto &[recorderId, recorder] : m_recorders)
		{
			recorder->Dispose();
		}
		registrar_->UnregisterTopLevelWindowProcDelegate(window_proc_delegate);
	}

	void AudioStreamer::RegisterWithRegistrar(PluginRegistrarWindows *registrar)
	{
		binary_messenger = registrar->messenger();

		auto methodChannel = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
			binary_messenger, "com.softigent.audiostreamer",
			&flutter::StandardMethodCodec::GetInstance());

		auto plugin = std::make_unique<AudioStreamer>(registrar);
		methodChannel->SetMethodCallHandler(
			[plugin_pointer = plugin.get()](const auto &call, auto result)
			{
				plugin_pointer->SetMethodCallHandler(call, std::move(result));
			});
		registrar->AddPlugin(std::move(plugin));

		// Set up function to retrieve the root HWND for message dispatching
		root_window_getter = [registrar]
		{
			return ::GetAncestor(registrar->GetView()->GetNativeWindow(), GA_ROOT);
		};

		// Register window proc delegate
		window_proc_delegate = registrar->RegisterTopLevelWindowProcDelegate(
			[plugin_ptr = plugin.get()](HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
			{
				if (message == WM_SETFONT) {
					delegate_callbacks.front()();
					delegate_callbacks.pop_front();
					return 0;
				}
				return NULL;
			});
	}

	void AudioStreamer::CallbackHandler(std::function<void()> callback)
	{
		delegate_callbacks.push_back(callback);
		PostMessage(root_window_getter(), WM_SETFONT, 0, 0);
	}

	void AudioStreamer::AudioStreamer::SetMethodCallHandler(
		const MethodCall<EncodableValue> &method_call,
		std::unique_ptr<MethodResult<EncodableValue>> result)
	{
		const auto args = method_call.arguments();
		const auto *params = std::get_if<EncodableMap>(args);
		if (!params)
		{
			ErrorMessage("Call missing parameters", *result);
			return;
		}

		std::string recorderId;
		GetValueFromEncodableMap(params, "recorderId", recorderId);
		if (recorderId.empty())
		{
			ErrorMessage("Call missing mandatory parameter recorderId", *result);
			return;
		}

		// Use the constexpr function instead of a lambda
		const std::string &method = method_call.method_name();
		const uint32_t methodHash = HashMethodName(method.c_str());
		HRESULT hr;

		if (methodHash == kCreate)
		{
			hr = CreateEventStreamHandlers(recorderId);
			SUCCEEDED(hr) ? result->Success() : ResultError(hr, *result);
			return;
		}

		auto searchedRecorder = m_recorders.find(recorderId);
		if (searchedRecorder == m_recorders.end())
		{
			ErrorMessage("Recorder with the given ID was not found.", *result);
			return;
		}
		auto recorder = searchedRecorder->second.get();
		if (!recorder)
		{
			ErrorMessage("Recorder instance is null.", *result);
			return;
		}

		switch (methodHash)
		{
		case kHasPermission:
			result->Success(EncodableValue(true));
			break;

		case kStart:
		{
			std::string deviceId;
			GetValueFromEncodableMap(params, "deviceId", deviceId);

			if (deviceId.length() != 0)
			{
				std::wstring deviceIdW = std::wstring(deviceId.begin(), deviceId.end());
				hr = recorder->Start(recorderId, deviceIdW.c_str());
			}
			else
			{
				hr = recorder->Start(recorderId, NULL);
			}
			if (SUCCEEDED(hr))
			{
				result->Success(EncodableValue());
			}
			else
			{
				ResultError(hr, *result);
			}
			break;
		}

		case kStop:
		{
			hr = recorder->Stop();
			if (SUCCEEDED(hr))
			{
				result->Success(EncodableValue());
			}
			else
			{
				ResultError(hr, *result);
			}
			break;
		}

		case kPause:
			hr = recorder->Pause();
			if (SUCCEEDED(hr))
			{
				result->Success(EncodableValue());
			}
			else
			{
				ResultError(hr, *result);
			}
			break;

		case kResume:
			hr = recorder->Resume();
			if (SUCCEEDED(hr))
			{
				result->Success(EncodableValue());
			}
			else
			{
				ResultError(hr, *result);
			}
			break;

		case kIsPaused:
			result->Success(EncodableValue(recorder->IsPaused()));
			break;

		case kIsRecording:
			result->Success(EncodableValue(recorder->IsRecording()));
			break;

		case kDispose:
		{
			auto it = m_recorders.find(recorderId);
			if (it != m_recorders.end())
			{
				it->second->Dispose();
				m_recorders.erase(it);
			}
			result->Success();
			break;
		}

		case kListDevices:
		{
			ListDevices(*result);
			break;
		}

		default:
			result->NotImplemented();
			break;
		}
	}

	HRESULT AudioStreamer::CreateEventStreamHandlers(std::string recorderId)
	{
		auto eventHandler = new EventStreamHandler<>();
		std::unique_ptr<StreamHandler<EncodableValue>> pStateEventHandler{static_cast<StreamHandler<EncodableValue> *>(eventHandler)};

		auto eventChannel = std::make_unique<EventChannel<EncodableValue>>(
			binary_messenger, "com.softigent.audiostreamer/events/" + recorderId,
			&StandardMethodCodec::GetInstance());
		eventChannel->SetStreamHandler(std::move(pStateEventHandler));

		auto eventRecordHandler = new EventStreamHandler<>();
		std::unique_ptr<StreamHandler<EncodableValue>> pRecordEventHandler{static_cast<StreamHandler<EncodableValue> *>(eventRecordHandler)};

		auto eventRecordChannel = std::make_unique<EventChannel<EncodableValue>>(
			binary_messenger, "com.softigent.audiostreamer/records/" + recorderId,
			&StandardMethodCodec::GetInstance());
		eventRecordChannel->SetStreamHandler(std::move(pRecordEventHandler));

		Recorder *raw_recorder = nullptr;
		HRESULT hr = Recorder::CreateInstance(eventHandler, eventRecordHandler, &raw_recorder);
		if (SUCCEEDED(hr))
		{
			m_recorders.insert(std::make_pair(recorderId, std::move(raw_recorder)));
		}

		return hr;
	}
} // namespace record_windows
