import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import 'package:socket_audiostreamer/mediarecorder.dart';
import 'package:socket_audiostreamer/mediaplayer.dart';

const HOST = "localhost:9000";

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

  late StreamController<Uint8List> _audioBufferController;
  StreamSubscription? _playerStreamSubscription;

  final _record = MediaRecorder();
  final _player = MediaPlayer();
  bool _isRecording = false;
  bool _isCreated = false;
  bool _isListening = false;
  double _volume = 1.0;
  dynamic _selectedInputDevice;
  dynamic _selectedOutputDevice;
  List<dynamic> _inputDevices = [];
  List<dynamic> _outputDevices = [];
  String status = "Initializing...";

  @override
  void initState() {
    super.initState();
    _audioBufferController = StreamController<Uint8List>(sync: false);
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
      final recording = await _record.isRecording();
      if (recording) {
        await _record.stop();
        setState(() {
          _isRecording = !recording;
          status = "Stopped.";
        });
        _recordChannel.sink.close();
      } else {
        _recordChannel = WebSocketChannel.connect(Uri.parse('ws://$HOST/recording'));
        dynamic device = _selectedInputDevice;
        print(device?["label"]);
        final stream = await _record.start(device?["id"]);
        stream.listen(
          (data) => _recordChannel.sink.add(data),
          onError: (e) => setState(() => status = "Stream error: $e"),
          onDone: () => setState(() => status = "Stream closed"),
        );
        setState(() {
          _isRecording = !recording;
          status = "Recording...";
        });
      }
    } catch (e) {
      setState(() => status = "Error: ${e.toString()}");
    }
  }

  Future<void> _handleListeningAction() async {
    try {
      if (_isListening) {
        await _player.stop();
        await _playerStreamSubscription?.cancel();
        _playerChannel.sink.close();
        _audioBufferController.close();
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
        _playerChannel.stream.listen(
          (data) {
            if (data is Uint8List && !_audioBufferController.isClosed) {
              _audioBufferController.add(data);
            }
          },
          onError: (e) => setState(() => status = "WebSocket error: $e"),
          onDone: () => setState(() => status = "WebSocket closed"),
        );

        // Drain buffer and feed to native player
        _playerStreamSubscription = _audioBufferController.stream.listen(
          (chunk) async {
            await _player.addChunk(chunk);
            await Future.delayed(Duration(milliseconds: 1)); // CPU-friendly pacing
          },
          onError: (e) => setState(() => status = "AddChunk error: $e"),
          cancelOnError: false,
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
            final paused = await _record.isPaused();
            setState(() {
              _isRecording = false;
              status = paused ? "Recording paused" : "Failed to pause recording";
            });
          },
          style: ElevatedButton.styleFrom(backgroundColor: Colors.orange),
        ),
        ElevatedButton.icon(
          label: const Text("Resume"),
          onPressed: () async {
            await _record.resume();
            final paused = await _record.isPaused();
            setState(() {
              _isRecording = true;
              status = !paused ? "Recording resumed" : "Failed to resume recording";
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
              onChanged: _isCreated
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
              _isCreated = false;
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
    _player.dispose();
    _recordChannel.sink.close();
    _playerChannel.sink.close();
    _audioBufferController.close();
    _playerStreamSubscription?.cancel();
    super.dispose();
  }
}
