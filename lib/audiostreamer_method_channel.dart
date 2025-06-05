import 'package:flutter/services.dart';

class AudioStreamerMethodChannel {

  final _methodChannel = const MethodChannel('com.softigent.audiostreamer');

  Future<void> create(String recorderId) {
    return _methodChannel.invokeMethod<void>(
      'create',
      {'recorderId': recorderId},
    );
  }

  Future<bool> hasPermission(String recorderId) async {
    final result = await _methodChannel.invokeMethod<bool>(
      'hasPermission',
      {'recorderId': recorderId},
    );
    return result ?? false;
  }

  Future<Stream<Uint8List>> start(String recorderId, String? deviceId) async {
    final eventRecordChannel = EventChannel(
      'com.softigent.audiostreamer/records/$recorderId',
    );

    await _methodChannel.invokeMethod('start', {
      'recorderId': recorderId,
      'deviceId': deviceId
    });

    return eventRecordChannel
        .receiveBroadcastStream()
        .map<Uint8List>((data) => data);
  }

  Future<String?> stop(String recorderId) async {
    final outputPath = await _methodChannel.invokeMethod(
      'stop',
      {'recorderId': recorderId},
    );

    return outputPath;
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
    final result = await _methodChannel.invokeMethod<bool>(
      'isPaused',
      {'recorderId': recorderId},
    );

    return result ?? false;
  }

  Future<bool> isRecording(String recorderId)  async {
    final result = await _methodChannel.invokeMethod<bool>(
      'isRecording',
      {'recorderId': recorderId},
    );
    return result ?? false;
  }

  Future<void> dispose(String recorderId)  async {
    await _methodChannel.invokeMethod(
      'dispose',
      {'recorderId': recorderId},
    );
  }

  Stream<dynamic> listen(String recorderId) {
    final eventChannel = EventChannel(
      'com.softigent.audiostreamer/events/$recorderId',
    );
    return eventChannel.receiveBroadcastStream();
  }

  Future<Map<Object?, Object?>> listDevices(String recorderId) async {
    return await _methodChannel.invokeMethod<Map<Object?, Object?>>(
      'listDevices',
      {'recorderId': recorderId},
    ) ?? {};
  }
}