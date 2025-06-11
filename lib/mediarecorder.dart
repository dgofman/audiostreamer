import 'dart:async';
import 'dart:typed_data';
import 'package:socket_audiostreamer/mediarecorder_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MediaRecorder extends PlatformInterface {
  static final Object _token = Object();

  static final _instance = MediaRecorderChannel();

  late String _recorderId;
  bool _created = false;

  // Constructor with PlatformInterface verification token
  MediaRecorder() : super(token: _token);

  Future<T> _create<T>(Future<T> Function() callback) async {
    if (!_created) {
      _recorderId = DateTime.now().millisecondsSinceEpoch.toString();
      await _instance.create(_recorderId);
      _created = true;
    }
    return callback();
  }

  Future<bool> hasPermission() async {
    return _create(() => _instance.hasPermission(_recorderId));
  }

  Future<void> start(String? deviceId) async {
    _create(() => _instance.start(_recorderId, deviceId));
  }

  Future<String?> stop() async {
    return _create(() => _instance.stop(_recorderId));
  }

  Future<void> pause() async {
    _create(() => _instance.pause(_recorderId));
  }

  Future<void> resume() async {
    _create(() => _instance.resume(_recorderId));
  }

  Future<bool> get isPaused async {
    return _create(() => _instance.isPaused(_recorderId));
  }

  Future<bool> get isReady async {
    return _create(() => _instance.isReady(_recorderId));
  }

  Stream<Uint8List> get stream {
    return _instance.stream(_recorderId);
  }

  Future<void> dispose() async {
    if (_created) {
      _created = false;
      await _instance.dispose(_recorderId);
    }
  }

  Future<List<dynamic>> listDevices() async {
    return _create(() => _instance.listDevices(_recorderId));
  }
}
