#include <flutter/method_channel.h>		   // flutter::MethodChannel
#include <flutter/standard_method_codec.h> // flutter::StandardMethodCodec

#include "mediaplayer.h"

constexpr uint32_t kCreate = HashMethodName("create");
constexpr uint32_t kHasPermission = HashMethodName("hasPermission");
constexpr uint32_t kStart = HashMethodName("start");
constexpr uint32_t kStop = HashMethodName("stop");
constexpr uint32_t kAddChunk = HashMethodName("addChunk");
constexpr uint32_t kVolume = HashMethodName("volume");
constexpr uint32_t kIsCreated = HashMethodName("isCreated");
constexpr uint32_t kIsReady = HashMethodName("isReady");
constexpr uint32_t kDispose = HashMethodName("dispose");
constexpr uint32_t kListDevices = HashMethodName("listDevices");

namespace
{
	int window_proc_delegate;
	std::function<HWND(void)> root_window_getter;
	std::deque<std::function<void()>> delegate_callbacks;
	std::map<std::string, std::unique_ptr<playback::Player>> m_players{};
	BinaryMessenger *binary_messenger = nullptr;
}

namespace playback
{
	MediaPlayer::MediaPlayer(PluginRegistrarWindows *registrar)
		: registrar_(registrar) {}

	MediaPlayer::~MediaPlayer()
	{
		for (const auto &[playerId, player] : m_players)
		{
			player->Dispose();
		}
		registrar_->UnregisterTopLevelWindowProcDelegate(window_proc_delegate);
	}

	void MediaPlayer::RegisterWithRegistrar(PluginRegistrarWindows *registrar)
	{
		binary_messenger = registrar->messenger();

		auto methodChannel = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
			binary_messenger, "com.softigent.audiostreamer.MediaPlayer",
			&flutter::StandardMethodCodec::GetInstance());

		auto plugin = std::make_unique<MediaPlayer>(registrar);
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
				if (message == WM_SETFONT)
				{
					delegate_callbacks.front()();
					delegate_callbacks.pop_front();
					return 0;
				}
				return NULL;
			});
	}

	void MediaPlayer::MediaPlayer::SetMethodCallHandler(
		const MethodCall<EncodableValue> &methodCall,
		std::unique_ptr<MethodResult<EncodableValue>> result)
	{
		const auto *arguments = std::get_if<flutter::EncodableMap>(methodCall.arguments());
		if (!arguments)
		{
			ErrorMessage("Expected map of arguments", *result);
			return;
		}

		std::string playerId;
		auto rId = arguments->find(flutter::EncodableValue("playerId"));
		if (rId != arguments->end() && std::holds_alternative<std::string>(rId->second))
		{
			playerId = std::get<std::string>(rId->second);
		}
		else
		{
			ErrorMessage("Call missing mandatory parameter playerId", *result);
			return;
		}

		// Use the constexpr function instead of a lambda
		const uint32_t methodHash = HashMethodName(methodCall.method_name().c_str());
		HRESULT hr;

		if (methodHash == kCreate)
		{
			hr = CreatePlayer(playerId);
			SUCCEEDED(hr) ? result->Success() : ResultError(hr, *result);
			return;
		}

		auto searchedPlayer = m_players.find(playerId);
		if (searchedPlayer == m_players.end())
		{
			ErrorMessage("Player with the given ID was not found.", *result);
			return;
		}
		auto player = searchedPlayer->second.get();
		if (!player)
		{
			ErrorMessage("Player instance is null.", *result);
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
			if (dId != arguments->end() && std::holds_alternative<std::string>(dId->second))
			{
				std::string deviceId = std::get<std::string>(dId->second);
				std::wstring deviceIdW = std::wstring(deviceId.begin(), deviceId.end());
				hr = player->Start(playerId, deviceIdW.c_str());
			}
			else
			{
				hr = player->Start(playerId, NULL);
			}
			break;
		}

		case kStop:
		{
			hr = player->Stop();
			break;
		}

		case kAddChunk:
		{
			auto bytes_it = arguments->find(flutter::EncodableValue("bytes"));
			if (bytes_it == arguments->end() || !std::holds_alternative<std::vector<uint8_t>>(bytes_it->second))
			{
				ErrorMessage("Missing or invalid 'bytes' parameter", *result);
				return;
			}
			const auto &bytes = std::get<std::vector<uint8_t>>(bytes_it->second);
			hr = player->AddChunk(bytes);
			break;
		}

		case kVolume:
		{
			auto val = arguments->find(flutter::EncodableValue("value"));
			if (val == arguments->end() || !std::holds_alternative<double>(val->second))
			{
				ErrorMessage("Missing or invalid 'value' parameter", *result);
				return;
			}
			double value = std::get<double>(val->second);
			float volume = std::min(static_cast<float>(value), 1.0f);
			hr = player->SetVolume(volume);
			break;
		}

		case kIsCreated:
			result->Success(EncodableValue(player->IsCreated()));
			return;

		case kIsReady:
			result->Success(EncodableValue(player->IsReady()));
			return;

		case kDispose:
		{
			auto it = m_players.find(playerId);
			if (it != m_players.end())
			{
				it->second->Dispose();
				m_players.erase(it);
			}
			result->Success();
			return;
		}

		case kListDevices:
		{
			EncodableMap resultMap;
			HRESULT hr = GetDevices(resultMap);
			if (SUCCEEDED(hr))
			{
				result->Success(EncodableValue(resultMap["outputs"]));
				return;
			}
			return;
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

	HRESULT MediaPlayer::CreatePlayer(std::string playerId)
	{
		Player *raw_player = nullptr;
		HRESULT hr = Player::CreateInstance(&raw_player);
		if (SUCCEEDED(hr))
		{
			m_players.insert(std::make_pair(playerId, std::move(raw_player)));
		}
		return hr;
	}
} // namespace playback
