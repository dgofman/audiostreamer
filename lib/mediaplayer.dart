import 'dart:async';
import 'dart:typed_data';
import 'package:socket_audiostream/mediaplayer_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MediaPlayer extends PlatformInterface {
  static final Object _token = Object();

  static final _instance = MediaPlayerChannel();

  late String _playerId;
  bool _created = false;

  // Constructor with PlatformInterface verification token
  MediaPlayer() : super(token: _token);

  Future<T> _create<T>(Future<T> Function() callback) async {
    if (!_created) {
      _playerId = DateTime.now().millisecondsSinceEpoch.toString();
      await _instance.create(_playerId);
      _created = true;
    }
    return callback();
  }

  Future<bool> hasPermission() async {
    return _create(() => _instance.hasPermission(_playerId));
  }

  Future<void> start(String? deviceId) async {
    _create(() => _instance.start(_playerId, deviceId));
  }

  Future<String?> stop() async {
    return _create(() => _instance.stop(_playerId));
  }

  Future<bool> addChunk(Uint8List data) async {
    if (_created) {
      await _instance.addChunk(_playerId, data);
    }
    return _created;
  }

  Future<void> setJitterRange(int min, int max) async {
    _create(() => _instance.setJitterRange(_playerId, min, max));
  }

  Future<void> volume(double value) async {
    _create(() => _instance.volume(_playerId, value));
  }

  Future<bool> get isCreated async {
    return _create(() => _instance.isCreated(_playerId));
  }

  Future<bool> get isReady async {
    return _create(() => _instance.isReady(_playerId));
  }

  Future<void> dispose() async {
    if (_created) {
      _created = false;
      await _instance.dispose(_playerId);
    }
  }

  Future<List<dynamic>> listDevices() async {
    return _create(() => _instance.listDevices(_playerId));
  }
}
