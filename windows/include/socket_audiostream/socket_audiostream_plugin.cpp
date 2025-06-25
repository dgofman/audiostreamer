#include <flutter/plugin_registrar_windows.h> // flutter::
#include "socket_audiostream_plugin.h"        // AudioStreamerPlugin
#include "recording/mediarecorder.h"          // recording::
#include "playback/mediaplayer.h"             // playback::

void SocketAudiostreamPluginRegisterWithRegistrar(FlutterDesktopPluginRegistrarRef prr)
{
    #ifdef STEREO
        std::cout << "SocketAudiostreamPlugin - STEREO" << std::endl;
    #else
        std::cout << "SocketAudiostreamPlugin - MONO" << std::endl;
    #endif

    auto registrar = flutter::PluginRegistrarManager::GetInstance()
                         ->GetRegistrar<flutter::PluginRegistrarWindows>(prr);
    recording::MediaRecorder::RegisterWithRegistrar(registrar);
    playback::MediaPlayer::RegisterWithRegistrar(registrar);
}
