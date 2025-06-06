# AudioStreamer

**AudioStreamer** is a Flutter plugin for **Windows** that enables **low-latency audio recording** using the native Media Foundation API.

> 🎙️ **Recording via WebSocket and microphone.**  
> 🔊 **Playback via WebSocket and speakers.**

---

## ✨ Features

- Start, pause, resume, and stop recording
- Access available microphone devices
- Native implementation using C++ and Media Foundation
- No external dependencies
- Windows-only support

---

## 🎙️ Voice Processing Support (AEC, NS, AGC)

This project uses **Media Foundation** to capture microphone input and includes support for:

- 🎧 **Acoustic Echo Cancellation (AEC)**
- 🔇 **Noise Suppression (NS)**
- 🎚️ **Automatic Gain Control (AGC)**

[Audio Renderer Attributes – Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/medfound/audio-renderer-attributes)
[MF_AUDIO_RENDERER_ATTRIBUTE_FLAGS attribute | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/medfound/mf-audio-renderer-attribute-flags-attribute)
[Alphabetical List of Media Foundation Attributes] (https://learn.microsoft.com/en-us/windows/win32/medfound/alphabetical-list-of-media-foundation-attributes)

## 🚀 Getting Started

Add the plugin to your Flutter project:

```yaml
dependencies:
  audiostreamer:
    path: ../audiostreamer
```

Then run:

```bash
flutter pub get
```

## Usage

```dart
import 'package:audiostreamer/mediarecorder.dart';

final recorder = MediaRecorder();

await recorder.start();
await recorder.pause();
await recorder.resume();
await recorder.stop();
```

## Testing the Plugin

If you're testing the plugin from source:

```bash
flutter config --enable-windows-desktop
flutter create example
cd example

# Add the plugin from local path
flutter pub add audiostreamer --path=../

# Optional: Add WebSocket and dev dependencies
flutter pub add web_socket_channel
flutter pub add plugin_platform_interface --dev

flutter clean
flutter pub get
flutter run -d windows
```

See the [example/](example/) directory for a working Flutter app that demonstrates plugin usage.

## Plugin Development Notes

If you're contributing to this plugin:

- Comment out the plugin block in `pubspec.yaml` to allow re-creating the scaffold:
```yaml
#flutter:
#  plugin:
#    platforms:
#      windows:
#        pluginClass: AudioStreamerPlugin
```

-  Delete the existing `example` folder if needed.

- Recreate project scaffolding:

```bash
flutter config --enable-windows-desktop
flutter create .
flutter clean
flutter pub get
flutter run -d windows
```

## Windows Notes

This plugin uses the native Windows Media Foundation API for:

- Low-latency audio capture
- High performance
- Native device access without third-party libraries

## License

This project is licensed under the [MIT License](LICENSE).
