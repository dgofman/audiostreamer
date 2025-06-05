import 'dart:async';
import 'dart:typed_data';
import 'package:audiostreamer/audiostreamer_method_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class Audiostreamer extends PlatformInterface {
  static final Object _token = Object();

  static final _instance = AudioStreamerMethodChannel();

  late String _recorderId;
  late StreamController? _streamController;
  StreamSubscription? _streamSubscription;
  bool _created = false;

  // Constructor with PlatformInterface verification token
  Audiostreamer() : super(token: _token);

  Future<T> _create<T>(Future<T> Function() callback) async {
    if (!_created) {
      _recorderId = DateTime.now().millisecondsSinceEpoch.toString();
      await _instance.create(_recorderId);
      _streamController = StreamController.broadcast();
      final stream = _instance.listen(_recorderId);
      _streamSubscription = stream.listen(
        (state) {
          if (_streamController?.hasListener ?? false) {
            _streamController?.add(state);
          }
        },
        onError: (error) {
          if (_streamController?.hasListener ?? false) {
            _streamController?.addError(error);
          }
        },
      );
      _created = true;
    }
    return callback();
  }

  Future<bool> hasPermission() async {
    return await _create(() => _instance.hasPermission(_recorderId));
  }

  Future<Stream<Uint8List>> start(String? deviceId) async {
    return await _create(() => _instance.start(_recorderId, deviceId));
  }

  Future<String?> stop() async {
    return await _create(() => _instance.stop(_recorderId));
  }

  Future<void> pause() async {
    return await _create(() => _instance.pause(_recorderId));
  }

  Future<void> resume() async {
    return await _create(() => _instance.resume(_recorderId));
  }

  Future<bool> isPaused() async {
    return await _create(() => _instance.isPaused(_recorderId));
  }

  Future<bool> isRecording() async {
    return await _create(() => _instance.isRecording(_recorderId));
  }

  Future<void> dispose() async {
    await _streamSubscription?.cancel();
    await _streamController?.close();
    _streamSubscription = null;
    if (_created) {
      _instance.dispose(_recorderId);
    }
    _created = false;
  }

  Future<Map<dynamic, dynamic>> listDevices() async {
    return await _create(() => _instance.listDevices(_recorderId));
  }
}
