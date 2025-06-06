#include <flutter/plugin_registrar_windows.h> // flutter::
#include "audio_streamer_plugin.h"            // AudioStreamerPlugin
#include "recording/mediarecorder.h"          // recording::
#include "playback/mediaplayer.h"             // playback::

void AudioStreamerPluginRegisterWithRegistrar(FlutterDesktopPluginRegistrarRef prr)
{
    auto registrar = flutter::PluginRegistrarManager::GetInstance()
                         ->GetRegistrar<flutter::PluginRegistrarWindows>(prr);
    recording::MediaRecorder::RegisterWithRegistrar(registrar);
    playback::MediaPlayer::RegisterWithRegistrar(registrar);
}
