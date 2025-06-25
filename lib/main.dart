import 'dart:async';
import 'dart:convert';
import 'dart:developer';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:web_socket_channel/io.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import 'package:socket_audiostream/mediarecorder.dart';
import 'package:socket_audiostream/mediaplayer.dart';

const BASE_URL = 'ws://localhost:9000/ws'; // see test/utils.py

void main() {
  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  MyApp({super.key});
  @override
  _MyWidgetState createState() => _MyWidgetState();
}

class _MyWidgetState extends State<MyApp> {
  final int _roomId;
  final String _clientId;
  final Map<String, dynamic> _headers;

  _MyWidgetState({int? roomId, Map<String, dynamic>? headers, String? clientId})
      : _roomId = roomId ?? 0,
        _headers = headers ?? {},
        _clientId = clientId ?? DateTime.now().millisecondsSinceEpoch.toString();

  late WebSocketChannel? _recordingSocket;
  late WebSocketChannel? _listeningSocket;
  StreamSubscription? _recordStreamSubscription;
  StreamSubscription? _listeningStreamSubscription;

  final _record = MediaRecorder();
  final _player = MediaPlayer();
  bool _isRecording = false;
  bool _isListening = false;
  bool _isStereo = false;
  bool _isDenoise = false;
  double _volume = 0.1;
  dynamic _selectedInputDevice;
  dynamic _selectedOutputDevice;
  List<dynamic> _inputDevices = [];
  List<dynamic> _outputDevices = [];
  String status = 'Initializing...';

  @override
  void initState() {
    super.initState();
    _initialize();
  }

  void _initialize() async {
    try {
      _inputDevices = await _record.listDevices();
      _outputDevices = await _player.listDevices();
      _isStereo = await _player.isStereo();
      _selectedInputDevice = _inputDevices.isNotEmpty ? _inputDevices[0] : null;
      _selectedOutputDevice = _outputDevices.isNotEmpty ? _outputDevices[0] : null;
      setState(() => status = 'Ready');
    } catch (e) {
      setState(() => status = 'Initialization failed: $e');
    }
  }

  Future<WebSocketChannel?> _openWebSocket(String role, int roomId) async {
    final websocketCompleter = Completer();
    Uri socketURL = Uri.parse(BASE_URL);
    try {
      socketURL = socketURL.replace(queryParameters: {'role': role, 'roomId': '${roomId}', 'clientId': '${_clientId}'});
      setState(() {
        status = 'WebSocket try connecting to $socketURL';
      });
      return IOWebSocketChannel(await WebSocket.connect(socketURL.toString(), headers: _headers));
    } catch (e) {
      setState(() {
        status = 'WebSocket $socketURL\nError: ${e is SocketException ? e.message : e}';
      });
      if (!websocketCompleter.isCompleted) {
        websocketCompleter.completeError(e);
      }
    } finally {
      if (!websocketCompleter.isCompleted) websocketCompleter.complete();
    }
    return null;
  }

  Future<void> stopRecording() async {
    _recordStreamSubscription?.cancel();
    _recordStreamSubscription = null;
    await _recordingSocket?.sink.close();
    await _record.stop();
    setState(() {
      _isRecording = false;
      status = 'Stopped recording';
    });
    log('AudioStream Recording socket closed');
  }

  Future<void> _handleRecordingAction() async {
    try {
      if (await _record.isReady) {
        await stopRecording();
      } else {
        _recordingSocket = await _openWebSocket('recording', _roomId);
        if (_recordingSocket == null) {
          return;
        }

        dynamic device = _selectedInputDevice;
        print(device?['label']);
        await _record.start(device?['id']);

        _recordingSocket!.stream.listen((_) {}, onDone: () => stopRecording());

        _recordStreamSubscription = _record.stream.listen(
                (data) => _recordingSocket?.sink.add(data),
            onError: (e, st) => log('AudioStream Recording socket error', error: e, stackTrace: st),
            onDone: () => stopRecording());

        setState(() {
          _isRecording = true;
          status = 'Recording...';
        });
      }
    } catch (e) {
      setState(() => status = 'Error: ${e.toString()}');
    }
  }

  Future<void> stopListening() async {
    _listeningStreamSubscription?.cancel();
    _listeningStreamSubscription = null;
    await _listeningSocket?.sink.close();
    await _player.stop();
    setState(() {
      _isListening = false;
      status = 'Stopped listening';
    });
    log('AudioStream Listening socket closed');
  }

  Future<void> _handleListeningAction() async {
    try {
      if (await _player.isReady) {
        await stopListening();
      } else {
        _listeningSocket = await _openWebSocket('listening', _roomId);
        if (_recordingSocket == null) {
          return;
        }

        dynamic device = _selectedOutputDevice;
        print(device?['label']);
        await _player.start(device?['id']);

        // Listen to WebSocket and buffer
        _listeningStreamSubscription = _listeningSocket!.stream.listen((event) async {
          try {
            Uint8List chunk;
            if (event is List<int>) {
              chunk = Uint8List.fromList(event);
            } else if (event is Uint8List) {
              chunk = event;
            } else {
              if (event is String) {
                final json = jsonDecode(event);
                if (json["jitterMinMs"] != null && json["jitterMaxMs"] != null) {
                  return await _player.setJitterRange(json["jitterMinMs"], json["jitterMaxMs"]);
                }
              }
              log('Unsupported data type from listening socket');
              return;
            }
            if (_listeningStreamSubscription != null) {
              bool success = false;
              try {
                success = await _player.addChunk(chunk);
              } finally {
                if (!success) await stopListening();
              }
            }
          } catch (e, st) {
            log('AudioStream Listening error', error: e, stackTrace: st);
          }
        },
            onError: (e, st) => log('AudioStream Listening socket error', error: e, stackTrace: st),
            onDone: () => stopListening());

        setState(() {
          _isListening = true;
          status = 'Listening...';
        });
      }
    } catch (e) {
      setState(() => status = 'Error: ${e.toString()}');
    }
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(title: const Text('Audio Stream Controller')),
        body: SingleChildScrollView(
          padding: EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              _buildStatusCard(),
              const SizedBox(height: 20),
              _buildDeviceList(),
              const SizedBox(height: 20),
              _recordControlButtons(),
              const SizedBox(height: 20),
              _playerControlButtons(),
              const SizedBox(height: 20),
              _playerDeboise(),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildStatusCard() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(
          'Status: $status',
          textAlign: TextAlign.center,
          style: const TextStyle(fontWeight: FontWeight.bold),
        )
      ],
    );
  }

  Widget _buildDeviceList() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(8.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Input Devices
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                const Text('🎤 Input Device', style: TextStyle(fontWeight: FontWeight.bold)),
                TextButton(
                  onPressed: () async {
                    final devices = await _record.listDevices();
                    setState(() {
                      _inputDevices = devices;
                      if (_inputDevices.isNotEmpty) {
                        _selectedInputDevice = _inputDevices[0];
                        status = 'Input devices refreshed';
                      } else {
                        _selectedInputDevice = null;
                        status = 'No input devices found';
                      }
                    });
                  },
                  child: const Text('🔄 Refresh', style: TextStyle(fontSize: 14)),
                ),
              ],
            ),
            DropdownButton<Object?>(
              isExpanded: true,
              icon: const Text('▼'),
              value: _selectedInputDevice?['id'],
              items: _inputDevices.map<DropdownMenuItem<String>>((device) {
                return DropdownMenuItem<String>(
                  value: device['id'],
                  child: Text(device['label'] ?? 'Unknown'),
                );
              }).toList(),
              onChanged: (val) {
                setState(() {
                  stopRecording();
                  _selectedInputDevice = _inputDevices.firstWhere((device) => device['id'] == val);
                  status = 'Selected Input Device: ${_selectedInputDevice['label']}';
                });
              },
            ),

            const SizedBox(height: 16),

            // Output Devices
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                const Text('🔈 Output Device', style: TextStyle(fontWeight: FontWeight.bold)),
                TextButton(
                  onPressed: () async {
                    final devices = await _player.listDevices();
                    setState(() {
                      _outputDevices = devices;
                      if (_outputDevices.isNotEmpty) {
                        _selectedOutputDevice = _outputDevices[0];
                        status = 'Output devices refreshed';
                      } else {
                        _selectedOutputDevice = null;
                        status = 'No output devices found';
                      }
                    });
                  },
                  child: const Text('🔄 Refresh', style: TextStyle(fontSize: 14)),
                ),
              ],
            ),
            DropdownButton<Object?>(
              isExpanded: true,
              icon: const Text('▼'),
              value: _selectedOutputDevice?['id'],
              items: _outputDevices.map<DropdownMenuItem<String>>((device) {
                return DropdownMenuItem<String>(
                  value: device['id'],
                  child: Text(device['label'] ?? 'Unknown'),
                );
              }).toList(),
              onChanged: (val) {
                setState(() {
                  stopListening();
                  _selectedOutputDevice = _outputDevices.firstWhere((device) => device['id'] == val);
                  status = 'Selected Output Device: ${_selectedOutputDevice['label']}';
                });
              },
            ),
          ],
        ),
      ),
    );
  }

  Widget _recordControlButtons() {
    return Wrap(
      spacing: 10,
      runSpacing: 10,
      alignment: WrapAlignment.center,
      children: [
        ElevatedButton.icon(
          label: Text(_isRecording ? 'Stop Recording' : 'Start Recording'),
          onPressed: _handleRecordingAction,
          style: ElevatedButton.styleFrom(backgroundColor: _isRecording ? Colors.red : Colors.green),
        ),
        ElevatedButton.icon(
          label: const Text('Pause'),
          onPressed: () async {
            await _record.pause();
            final isPaused = await _record.isPaused;
            setState(() {
              _isRecording = false;
              status = isPaused ? 'Recording paused' : 'Failed to pause recording';
            });
          },
          style: ElevatedButton.styleFrom(backgroundColor: Colors.orange),
        ),
        ElevatedButton.icon(
          label: const Text('Resume'),
          onPressed: () async {
            await _record.resume();
            final isPaused = await _record.isPaused;
            setState(() {
              _isRecording = true;
              status = !isPaused ? 'Recording resumed' : 'Failed to resume recording';
            });
          },
          style: ElevatedButton.styleFrom(backgroundColor: Colors.blue),
        ),
        ElevatedButton.icon(
          label: const Text('Dispose'),
          onPressed: stopRecording,
          style: ElevatedButton.styleFrom(backgroundColor: Colors.grey),
        ),
      ],
    );
  }

  Widget _playerControlButtons() {
    return Wrap(
      spacing: 10,
      runSpacing: 10,
      alignment: WrapAlignment.center,
      children: [
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('🔊 Volume'),
            Slider(
              value: _volume,
              min: 0.0,
              max: 1.0,
              onChanged: _isListening
                  ? (val) async {
                      setState(() => _volume = val);
                      await _player.volume(_volume);
                    }
                  : null,
            ),
          ],
        ),
        ElevatedButton.icon(
          label: Text(_isListening ? 'Stop Listening' : 'Start Listening'),
          onPressed: _handleListeningAction,
          style: ElevatedButton.styleFrom(backgroundColor: _isListening ? Colors.red : Colors.green),
        ),
        ElevatedButton.icon(
          label: const Text('Dispose'),
          onPressed: stopListening,
          style: ElevatedButton.styleFrom(backgroundColor: Colors.grey),
        ),
      ],
    );
  }

  Widget _playerDeboise() {
    return Wrap(
      spacing: 10,
      runSpacing: 10,
      children: [
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('🪄 Denoise'),
            Switch(
              value: _isDenoise,
              onChanged: _isStereo ? null : (val) {
                _player.setDenoise(val);
                setState(() => _isDenoise = val);
              },
            ),
          ],
        ),
      ],
    );
  }

  @override
  void dispose() {
    stopRecording();
    stopListening();
    super.dispose();
  }
}
