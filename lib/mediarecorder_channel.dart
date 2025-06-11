import 'package:flutter/services.dart';

class MediaRecorderChannel {
  final _methodChannel = const MethodChannel('com.softigent.audiostream.MediaRecorder');

  Future<void> create(String recorderId) {
    return _methodChannel.invokeMethod<void>(
      'create',
      {'recorderId': recorderId},
    );
  }

  Future<bool> hasPermission(String recorderId) async {
    return (await _methodChannel.invokeMethod<bool>(
      'hasPermission',
      {'recorderId': recorderId},
    ) ?? false);
  }

  Future<void> start(String recorderId, String? deviceId) async {
    await _methodChannel.invokeMethod('start', {'recorderId': recorderId, 'deviceId': deviceId});
  }

  Future<String?> stop(String recorderId) async {
    return await _methodChannel.invokeMethod(
      'stop',
      {'recorderId': recorderId},
    );
  }

  Future<void> pause(String recorderId) {
    return _methodChannel.invokeMethod(
      'pause',
      {'recorderId': recorderId},
    );
  }

  Future<void> resume(String recorderId) {
    return _methodChannel.invokeMethod(
      'resume',
      {'recorderId': recorderId},
    );
  }

  Future<bool> isPaused(String recorderId) async {
    return (await _methodChannel.invokeMethod<bool>(
      'isPaused',
      {'recorderId': recorderId},
    ) ?? false);
  }

  Future<bool> isReady(String recorderId) async {
    return (await _methodChannel.invokeMethod<bool>(
      'isReady',
      {'recorderId': recorderId},
    ) ?? false);
  }

  Future<void> dispose(String recorderId) async {
    await _methodChannel.invokeMethod(
      'dispose',
      {'recorderId': recorderId},
    );
  }

  Stream<Uint8List> stream(String recorderId) {
    final recordEventChannel = EventChannel(
      'com.softigent.audiostream/recordEvent/$recorderId',
    );
    return recordEventChannel.receiveBroadcastStream().map<Uint8List>((data) => data);
  }

  Future<List<dynamic>> listDevices(String recorderId) async {
    return await _methodChannel.invokeMethod<dynamic>(
          'listDevices',
          {'recorderId': recorderId},
        ) ?? [];
  }
}
