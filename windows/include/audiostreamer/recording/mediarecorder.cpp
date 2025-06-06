#include <deque>

#include <flutter/method_channel.h>		   // flutter::MethodChannel
#include <flutter/standard_method_codec.h> // flutter::StandardMethodCodec

#include "mediarecorder.h"

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
	MediaRecorder::MediaRecorder(PluginRegistrarWindows* registrar)
    : registrar_(registrar) {}

	MediaRecorder::~MediaRecorder()
	{
		for (const auto &[recorderId, recorder] : m_recorders)
		{
			recorder->Dispose();
		}
		registrar_->UnregisterTopLevelWindowProcDelegate(window_proc_delegate);
	}

	void MediaRecorder::RegisterWithRegistrar(PluginRegistrarWindows *registrar)
	{
		binary_messenger = registrar->messenger();

		auto methodChannel = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
			binary_messenger, "com.softigent.audiostreamer.MediaRecorder",
			&flutter::StandardMethodCodec::GetInstance());

		auto plugin = std::make_unique<MediaRecorder>(registrar);
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

	void MediaRecorder::CallbackHandler(std::function<void()> callback)
	{
		delegate_callbacks.push_back(callback);
		PostMessage(root_window_getter(), WM_SETFONT, 0, 0);
	}

	void MediaRecorder::MediaRecorder::SetMethodCallHandler(
		const MethodCall<EncodableValue> &methodCall,
		std::unique_ptr<MethodResult<EncodableValue>> result)
	{
		const auto* arguments = std::get_if<flutter::EncodableMap>(methodCall.arguments());
		if (!arguments) {
			ErrorMessage("Expected map of arguments", *result);
			return;
		}

		std::string recorderId;
		auto rId = arguments->find(flutter::EncodableValue("recorderId"));
		if (rId != arguments->end() && std::holds_alternative<std::string>(rId->second)) {
			recorderId = std::get<std::string>(rId->second);
		} else {
			ErrorMessage("Call missing mandatory parameter recorderId", *result);
			return;
		}

		// Use the constexpr function instead of a lambda
		const uint32_t methodHash = HashMethodName(methodCall.method_name().c_str());
		HRESULT hr;

		if (methodHash == kCreate)
		{
			hr = CreateRecorder(recorderId);
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
			return;

		case kStart:
		{
			auto dId = arguments->find(flutter::EncodableValue("deviceId"));
			if (dId != arguments->end() && std::holds_alternative<std::string>(dId->second)) {
				std::string deviceId = std::get<std::string>(dId->second);
				std::wstring deviceIdW = std::wstring(deviceId.begin(), deviceId.end());
				hr = recorder->Start(recorderId, deviceIdW.c_str());
			} else {
				hr = recorder->Start(recorderId, NULL);
			}
			break;
		}

		case kStop:
		{
			hr = recorder->Stop();
			break;
		}

		case kPause:
			hr = recorder->Pause();
			break;

		case kResume:
			hr = recorder->Resume();
			break;

		case kIsPaused:
			result->Success(EncodableValue(recorder->IsPaused()));
			return;

		case kIsRecording:
			result->Success(EncodableValue(recorder->IsRecording()));
			return;

		case kDispose:
		{
			auto it = m_recorders.find(recorderId);
			if (it != m_recorders.end())
			{
				it->second->Dispose();
				m_recorders.erase(it);
			}
			result->Success();
			return;
		}

		case kListDevices:
		{
			EncodableMap resultMap;
			HRESULT hr = GetDevices(resultMap);
			if (SUCCEEDED(hr)) {
				result->Success(EncodableValue(resultMap["inputs"]));
				return;
			}
			break;
		}

		default:
			result->NotImplemented();
			return;
		}

		// Return Status
		if (SUCCEEDED(hr))
		{
			result->Success(EncodableValue());
		}
		else
		{
			ResultError(hr, *result);
		}
	}

	HRESULT MediaRecorder::CreateRecorder(std::string recorderId)
	{
		auto recordEventHandler = new EventStreamHandler<>();
		std::unique_ptr<StreamHandler<EncodableValue>> pRecordEventHandler{static_cast<StreamHandler<EncodableValue> *>(recordEventHandler)};

		auto recordEventChannel = std::make_unique<EventChannel<EncodableValue>>(
			binary_messenger, "com.softigent.audiostreamer/recordEvent/" + recorderId,
			&StandardMethodCodec::GetInstance());
		recordEventChannel->SetStreamHandler(std::move(pRecordEventHandler));

		Recorder *raw_recorder = nullptr;
		HRESULT hr = Recorder::CreateInstance(recordEventHandler, &raw_recorder);
		if (SUCCEEDED(hr))
		{
			m_recorders.insert(std::make_pair(recorderId, std::move(raw_recorder)));
		}

		return hr;
	}
} // namespace record
