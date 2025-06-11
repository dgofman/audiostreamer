import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import 'package:socket_audiostreamer/mediarecorder.dart';
import 'package:socket_audiostreamer/mediaplayer.dart';

const HOST = "localhost:50000";

void main() {
  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  MyApp({super.key});
  @override
  _MyWidgetState createState() => _MyWidgetState();
}

class _MyWidgetState extends State<MyApp> {
  late WebSocketChannel _recordChannel;
  late WebSocketChannel _playerChannel;

  StreamSubscription? _recordStreamSubscription;
  StreamSubscription? _playerStreamSubscription;

  final _record = MediaRecorder();
  final _player = MediaPlayer();
  bool _isRecording = false;
  bool _isListening = false;
  double _volume = 0.1;
  dynamic _selectedInputDevice;
  dynamic _selectedOutputDevice;
  List<dynamic> _inputDevices = [];
  List<dynamic> _outputDevices = [];
  String status = "Initializing...";

  @override
  void initState() {
    super.initState();
    _initialize();
  }

  void _initialize() async {
    try {
      _inputDevices = await _record.listDevices();
      _outputDevices = await _player.listDevices();
      _selectedInputDevice = _inputDevices.isNotEmpty ? _inputDevices[0] : null;
      _selectedOutputDevice = _outputDevices.isNotEmpty ? _outputDevices[0] : null;
      setState(() => status = "Ready");
    } catch (e) {
      setState(() => status = "Initialization failed: $e");
    }
  }

  Future<void> _handleRecordingAction() async {
    try {
      if (await _record.isReady) {
        await _record.stop();
        _recordStreamSubscription?.cancel();
        _recordChannel.sink.close();
        setState(() {
          _isRecording = false;
          status = "Stopped.";
        });
      } else {
        _recordChannel = WebSocketChannel.connect(Uri.parse('ws://$HOST/recording'));
        dynamic device = _selectedInputDevice;
        print(device?["label"]);
        await _record.start(device?["id"]);

        _recordStreamSubscription = _record.stream.listen(
              (data) => _recordChannel.sink.add(data),
              onError: (e) => setState(() => status = "WebSocket error: $e"),
              onDone: () => setState(() => status = "WebSocket closed"),
            );
        setState(() {
          _isRecording = true;
          status = "Recording...";
        });
      }
    } catch (e) {
      setState(() => status = "Error: ${e.toString()}");
    }
  }

  Future<void> _handleListeningAction() async {
    try {
      if (await _player.isReady) {
        await _player.stop();
        await _playerStreamSubscription?.cancel();
        _playerChannel.sink.close();
        setState(() {
          _isListening = false;
          status = "Stopped listening";
        });
      } else {
        _playerChannel = WebSocketChannel.connect(Uri.parse('ws://$HOST/listening'));
        dynamic device = _selectedOutputDevice;
        print(device?["label"]);
        await _player.start(device?["id"]);

        // Listen to WebSocket and buffer
        _playerStreamSubscription = _playerChannel.stream.listen(
          (data) async {
            if (data is Uint8List) {
              if (!await _player.addChunk(data)) {
                _playerStreamSubscription?.cancel();
              }
            }
          },
          onError: (e) => setState(() => status = "WebSocket error: $e"),
          onDone: () => setState(() => status = "WebSocket closed"),
        );

        setState(() {
          _isListening = true;
          status = "Listening...";
        });
      }
    } catch (e) {
      setState(() => status = "Error: ${e.toString()}");
    }
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(title: const Text("Audio Stream Controller")),
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
          "Status: $status",
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
            const Text("🎤 Input Device", style: TextStyle(fontWeight: FontWeight.bold)),
            DropdownButton<Object?>(
              isExpanded: true,
              value: _selectedInputDevice?['id'],
              icon: const Text("v"),
              items: _inputDevices.map<DropdownMenuItem<String>>((device) {
                return DropdownMenuItem<String>(
                  value: device['id'],
                  child: Text(device['label'] ?? 'Unknown'),
                );
              }).toList(),
              onChanged: (val) {
                setState(() {
                  _selectedInputDevice = _inputDevices.firstWhere((device) => device['id'] == val);
                  status = "Selected Device - ${_selectedInputDevice['label']}";
                  _isRecording = false;
                });
              },
            ),
            const SizedBox(height: 16),
            const Text("🔈 Output Device", style: TextStyle(fontWeight: FontWeight.bold)),
            DropdownButton<Object?>(
              isExpanded: true,
              value: _selectedOutputDevice?['id'],
              icon: const Text("v"),
              items: _outputDevices.map<DropdownMenuItem<String>>((device) {
                return DropdownMenuItem<String>(
                  value: device['id'],
                  child: Text(device['label'] ?? 'Unknown'),
                );
              }).toList(),
              onChanged: (val) {
                setState(() {
                  _selectedOutputDevice = _outputDevices.firstWhere((device) => device['id'] == val);
                  status = "Selected Device - ${_selectedOutputDevice['label']}";
                  _isListening = false;
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
          label: Text(_isRecording ? "Stop Recording" : "Start Recording"),
          onPressed: _handleRecordingAction,
          style: ElevatedButton.styleFrom(backgroundColor: _isRecording ? Colors.red : Colors.green),
        ),
        ElevatedButton.icon(
          label: const Text("Pause"),
          onPressed: () async {
            await _record.pause();
            setState(() async {
              _isRecording = false;
              status = await _record.isPaused ? "Recording paused" : "Failed to pause recording";
            });
          },
          style: ElevatedButton.styleFrom(backgroundColor: Colors.orange),
        ),
        ElevatedButton.icon(
          label: const Text("Resume"),
          onPressed: () async {
            await _record.resume();
            setState(() async {
              _isRecording = true;
              status = !await _record.isPaused? "Recording resumed" : "Failed to resume recording";
            });
          },
          style: ElevatedButton.styleFrom(backgroundColor: Colors.blue),
        ),
        ElevatedButton.icon(
          label: const Text("Dispose"),
          onPressed: () async {
            await _record.dispose();
            setState(() {
              _isRecording = false;
              status = "Recorder disposed";
            });
          },
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
            const Text("🔊 Volume"),
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
          label: Text(_isListening ? "Stop Listening" : "Start Listening"),
          onPressed: _handleListeningAction,
          style: ElevatedButton.styleFrom(backgroundColor: _isListening ? Colors.red : Colors.green),
        ),
        ElevatedButton.icon(
          label: const Text("Dispose"),
          onPressed: () async {
            await _player.dispose();
            setState(() {
              _isListening = false;
              status = "Player disposed";
            });
          },
          style: ElevatedButton.styleFrom(backgroundColor: Colors.grey),
        ),
      ],
    );
  }

  @override
  void dispose() {
    _record.dispose();
    _recordChannel.sink.close();
    _recordStreamSubscription?.cancel();

    _player.dispose();
    _playerChannel.sink.close();
    _playerStreamSubscription?.cancel();
    super.dispose();
  }
}
