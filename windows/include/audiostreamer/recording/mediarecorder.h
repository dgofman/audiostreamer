#ifndef FLUTTER_PLUGIN_MEDIA_RECORDER_H_
#define FLUTTER_PLUGIN_MEDIA_RECORDER_H_

#include <flutter/plugin_registrar_windows.h> // flutter::Plugin, flutter::PluginRegistrarWindows

#include "recorder.h"

using namespace flutter;

namespace recording
{
	class MediaRecorder : public flutter::Plugin
	{
	public:
		explicit MediaRecorder(flutter::PluginRegistrarWindows *registrar);
		virtual ~MediaRecorder();
		static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);
		static void CallbackHandler(std::function<void()> callback);

	private:
		flutter::PluginRegistrarWindows *registrar_; // store registrar
		void SetMethodCallHandler(const MethodCall<EncodableValue> &method_call,
								  std::unique_ptr<MethodResult<EncodableValue>> result);
		HRESULT CreateRecorder(std::string recorderId);
	};

} // namespace recording

#endif // FLUTTER_PLUGIN_MEDIA_RECORDER_H_
