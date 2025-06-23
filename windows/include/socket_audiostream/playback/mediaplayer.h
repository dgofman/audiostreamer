#ifndef FLUTTER_PLUGIN_MEDIA_PLAYER_H_
#define FLUTTER_PLUGIN_MEDIA_PLAYER_H_

#include <flutter/method_call.h>      // MethodCall
#include <flutter/method_result.h>    // MethodResult
#include <flutter/plugin_registrar_windows.h> // flutter::Plugin, flutter::PluginRegistrarWindows

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

	private:
		flutter::PluginRegistrarWindows *registrar_; // store registrar
		void SetMethodCallHandler(const MethodCall<EncodableValue> &method_call,
								  std::unique_ptr<MethodResult<EncodableValue>> result);
		HRESULT CreatePlayer(std::string playerId);
	};

} // namespace playback

#endif // FLUTTER_PLUGIN_MEDIA_PLAYER_H_
