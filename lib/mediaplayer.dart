import 'dart:async';
import 'dart:typed_data';
import 'package:socket_audiostreamer/mediaplayer_channel.dart';
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
    return await _create(() => _instance.hasPermission(_playerId));
  }

  Future<void> start(String? deviceId) async {
    return await _create(() => _instance.start(_playerId, deviceId));
  }

  Future<String?> stop() async {
    return await _create(() => _instance.stop(_playerId));
  }

  Future<void> addChunk(Uint8List data) async {
    if (_created) {
      await _instance.addChunk(_playerId, data);
    }
  }

  Future<void> volume(double value) async {
    return await _create(() => _instance.volume(_playerId, value));
  }

  Future<bool> isCreated() async {
    return await _create(() => _instance.isCreated(_playerId));
  }

  Future<bool> isListening() async {
    return await _create(() => _instance.isListening(_playerId));
  }

  Future<void> dispose() async {
    if (_created) {
      _instance.dispose(_playerId);
    }
    _created = false;
  }

  Future<List<dynamic>> listDevices() async {
    return await _create(() => _instance.listDevices(_playerId));
  }
}
