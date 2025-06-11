import 'package:flutter/services.dart';

class MediaPlayerChannel {
  final _methodChannel = const MethodChannel('com.softigent.audiostream.MediaPlayer');

  Future<void> create(String playerId) {
    return _methodChannel.invokeMethod<void>(
      'create',
      {'playerId': playerId},
    );
  }

  Future<bool> hasPermission(String playerId) async {
    return (await _methodChannel.invokeMethod<bool>(
      'hasPermission',
      {'playerId': playerId},
    ) ?? false);
  }

  Future<void> start(String playerId, String? deviceId) async {
    await _methodChannel.invokeMethod('start', {'playerId': playerId, 'deviceId': deviceId});
  }

  Future<String?> stop(String playerId) async {
    return await _methodChannel.invokeMethod(
      'stop',
      {'playerId': playerId},
    );
  }

  Future<void> addChunk(String playerId, Uint8List data) async {
    await _methodChannel.invokeMethod('addChunk', {
      'playerId': playerId,
      'bytes': data,
    });
  }

  Future<void> volume(String playerId, double value) {
    return _methodChannel.invokeMethod('volume', {'playerId': playerId, 'value': value});
  }

  Future<bool> isCreated(String playerId) async {
    return (await _methodChannel.invokeMethod<bool>(
      'isCreated',
      {'playerId': playerId},
    ) ?? false);
  }

  Future<bool> isReady(String playerId) async {
    return (await _methodChannel.invokeMethod<bool>(
      'isReady',
      {'playerId': playerId},
    ) ?? false);
  }

  Future<void> dispose(String playerId) async {
    await _methodChannel.invokeMethod(
      'dispose',
      {'playerId': playerId},
    );
  }

  Future<List<dynamic>> listDevices(String playerId) async {
    return await _methodChannel.invokeMethod<dynamic>(
          'listDevices',
          {'playerId': playerId},
        ) ?? [];
  }
}
