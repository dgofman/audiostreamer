#ifndef FLUTTER_PLUGIN_MEDIA_PLAYER_H_
#define FLUTTER_PLUGIN_MEDIA_PLAYER_H_

#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/method_channel.h>
#include "player.h"

using namespace flutter;

namespace playback
{
	class MediaPlayer : public flutter::Plugin
	{
	public:
		explicit MediaPlayer(flutter::PluginRegistrarWindows *registrar);
		virtual ~MediaPlayer();
		static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);
		static void CallbackHandler(std::function<void()> callback);

	private:
		flutter::PluginRegistrarWindows *registrar_; // store registrar
		void SetMethodCallHandler(const MethodCall<EncodableValue> &method_call,
								  std::unique_ptr<MethodResult<EncodableValue>> result);
		HRESULT CreateEventStreamHandlers(std::string playerId);
	};

} // namespace playback

#endif // FLUTTER_PLUGIN_MEDIA_PLAYER_H_
