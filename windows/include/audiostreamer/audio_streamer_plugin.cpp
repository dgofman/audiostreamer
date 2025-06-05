#include <flutter/plugin_registrar_windows.h> // flutter::
#include "audio_streamer_plugin.h"            // AudioStreamerPlugin
#include "recording/audiostreamer.h"          // recording::

void AudioStreamerPluginRegisterWithRegistrar(FlutterDesktopPluginRegistrarRef prr)
{
    auto registrar = flutter::PluginRegistrarManager::GetInstance()
                         ->GetRegistrar<flutter::PluginRegistrarWindows>(prr);
    recording::AudioStreamer::RegisterWithRegistrar(registrar);
}
