#pragma once

#include <comdef.h>						   // _com_error
#include <mfapi.h>						   // MFCreateAttributes
#include <mfidl.h>						   // MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MFEnumDeviceSources
#include <mmdeviceapi.h>				   // IMMDeviceEnumerator, IMMDevice
#include <functiondiscoverykeys_devpkey.h> // PKEY_Device_FriendlyName
#include <wrl/client.h>					   // Microsoft::WRL::ComPtr
#include <flutter/event_channel.h>		   // flutter::MethodResult
#include <flutter/encodable_value.h>	   // flutter::EncodableValue

#define NOMINMAX

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))

using Microsoft::WRL::ComPtr; // Enables ComPtr smart pointer usage

// Debug flag
constexpr bool DEBUG = false;

// Central debug logger
inline void DebugPrint(const char* fmt, ...)
{
	if (!DEBUG) return;
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	fflush(stdout);
}

template <class T>
inline void SafeRelease(T *&pT)
{
	if (pT != NULL)
	{
		pT->Release();
		pT = NULL;
	}
}

constexpr uint32_t HashMethodName(const char *str)
{
	uint32_t hash = 2166136261u;
	while (*str)
	{
		hash ^= static_cast<uint8_t>(*str++);
		hash *= 16777619u;
	}
	return hash;
}

inline std::string ToUtf8(const wchar_t *utf16_string)
{
	unsigned int target_length = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, utf16_string,
													   -1, nullptr, 0, nullptr, nullptr) -
								 1;
	int input_length = (int)wcslen(utf16_string);
	std::string utf8_string;
	utf8_string.resize(target_length);
	int converted_length = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, utf16_string,
												 input_length, utf8_string.data(), target_length, nullptr, nullptr);
	return converted_length == 0 ? std::string() : utf8_string;
}

inline void ErrorMessage(const std::string &error_message, flutter::MethodResult<flutter::EncodableValue> &result)
{
	result.Error("AudioStreamer", "Error", flutter::EncodableValue(error_message));
}

inline void ResultError(HRESULT hr, flutter::MethodResult<flutter::EncodableValue> &result)
{
	_com_error err(hr);
	ErrorMessage(ToUtf8(err.ErrorMessage()), result);
}

inline HRESULT GetDevices(flutter::EncodableMap &resultMap)
{
	flutter::EncodableList inputDevices;
	flutter::EncodableList outputDevices;

	// -------- INPUT DEVICES --------
	IMFAttributes *pAttributes = nullptr;
	IMFActivate **ppDevices = nullptr;
	UINT32 count = 0;

	HRESULT hr = MFCreateAttributes(&pAttributes, 1);
	if (SUCCEEDED(hr))
	{
		hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
	}

	if (SUCCEEDED(hr))
	{
		hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
	}

	if (SUCCEEDED(hr))
	{
		for (UINT32 i = 0; i < count; ++i)
		{
			LPWSTR id = nullptr;
			LPWSTR name = nullptr;
			UINT32 idLen = 0, nameLen = 0;

			if (SUCCEEDED(ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID, &id, &idLen)) &&
				SUCCEEDED(ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &nameLen)))
			{
				inputDevices.push_back(flutter::EncodableMap{
					{flutter::EncodableValue("id"), flutter::EncodableValue(ToUtf8(id))},
					{flutter::EncodableValue("label"), flutter::EncodableValue(ToUtf8(name))},
				});
				CoTaskMemFree(id);
				CoTaskMemFree(name);
			}
		}

		for (UINT32 i = 0; i < count; ++i)
		{
			SafeRelease(ppDevices[i]);
		}
		CoTaskMemFree(ppDevices);
	}

	SafeRelease(pAttributes);

	// -------- OUTPUT DEVICES --------
	ComPtr<IMMDeviceEnumerator> pEnumerator;
	ComPtr<IMMDeviceCollection> pCollection;
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
						  __uuidof(IMMDeviceEnumerator), (void **)&pEnumerator);

	if (SUCCEEDED(hr))
	{
		hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
	}

	if (SUCCEEDED(hr))
	{
		UINT deviceCount = 0;
		hr = pCollection->GetCount(&deviceCount);
		if (SUCCEEDED(hr))
		{
			for (UINT i = 0; i < deviceCount; i++)
			{
				ComPtr<IMMDevice> pDevice;
				LPWSTR id = nullptr;
				ComPtr<IPropertyStore> pProps;
				PROPVARIANT varName;

				if (SUCCEEDED(pCollection->Item(i, &pDevice)) &&
					SUCCEEDED(pDevice->GetId(&id)) &&
					SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps)) &&
					SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName)))
				{
					outputDevices.push_back(flutter::EncodableMap{
						{flutter::EncodableValue("id"), flutter::EncodableValue(ToUtf8(id))},
						{flutter::EncodableValue("label"), flutter::EncodableValue(ToUtf8(varName.pwszVal))},
					});
					CoTaskMemFree(id);
					PropVariantClear(&varName);
				}
			}
		}
	}

	// -------- Combine & Return --------
	resultMap = {
		{flutter::EncodableValue("inputs"), flutter::EncodableValue(inputDevices)},
		{flutter::EncodableValue("outputs"), flutter::EncodableValue(outputDevices)},
	};

	return hr;
}

// EventStreamHandler
template <typename T = flutter::EncodableValue>
class EventStreamHandler : public flutter::StreamHandler<T>
{
public:
	EventStreamHandler() = default;

	virtual ~EventStreamHandler() = default;

	void Success(std::unique_ptr<T> _data)
	{
		if (m_sink.get())
			m_sink.get()->Success(*_data.get());
	}

protected:
	std::unique_ptr<flutter::StreamHandlerError<T>> OnListenInternal(
		const T *arguments, std::unique_ptr<flutter::EventSink<T>> &&events) override
	{
		m_sink = std::move(events);
		return nullptr;
	}

	std::unique_ptr<flutter::StreamHandlerError<T>> OnCancelInternal(
		const T *arguments) override
	{
		m_sink.release();
		return nullptr;
	}

private:
	std::unique_ptr<flutter::EventSink<T>> m_sink;
};