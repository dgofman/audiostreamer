import 'dart:async';
import 'dart:typed_data';
import 'package:socket_audiostreamer/mediarecorder_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MediaRecorder extends PlatformInterface {
  static final Object _token = Object();

  static final _instance = MediaRecorderChannel();

  late String _recorderId;
  late StreamController? _stateController;
  StreamSubscription? _stateSubscription;
  bool _created = false;

  // Constructor with PlatformInterface verification token
  MediaRecorder() : super(token: _token);

  Future<T> _create<T>(Future<T> Function() callback) async {
    if (!_created) {
      _recorderId = DateTime.now().millisecondsSinceEpoch.toString();
      await _instance.create(_recorderId);
      _stateController = StreamController.broadcast();
      _stateSubscription = _instance.listen(_recorderId).listen(
        (state) {
          if (_stateController?.hasListener ?? false) {
            _stateController?.add(state);
          }
        },
        onError: (error) {
          if (_stateController?.hasListener ?? false) {
            _stateController?.addError(error);
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
    await _stateSubscription?.cancel();
    await _stateController?.close();
    _stateSubscription = null;
    if (_created) {
      _instance.dispose(_recorderId);
    }
    _created = false;
  }

  Future<List<dynamic>> listDevices() async {
    return await _create(() => _instance.listDevices(_recorderId));
  }
}
