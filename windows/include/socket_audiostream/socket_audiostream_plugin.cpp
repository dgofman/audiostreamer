#include <flutter/plugin_registrar_windows.h> // flutter::
#include "socket_audiostream_plugin.h"            // AudioStreamerPlugin
#include "recording/mediarecorder.h"          // recording::
#include "playback/mediaplayer.h"             // playback::

#include "utils.h" // SAMPLE_RATE and CHANNELS

void SocketAudiostreamPluginRegisterWithRegistrar(FlutterDesktopPluginRegistrarRef prr)
{
    std::wcout << L"SocketAudiostreamPlugin - Channels: " << CHANNELS << ", SampleRate: " << SAMPLE_RATE << std::endl;

    auto registrar = flutter::PluginRegistrarManager::GetInstance()
                         ->GetRegistrar<flutter::PluginRegistrarWindows>(prr);
    recording::MediaRecorder::RegisterWithRegistrar(registrar);
    playback::MediaPlayer::RegisterWithRegistrar(registrar);
}
