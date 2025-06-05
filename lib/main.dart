import 'package:flutter/material.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import 'package:audiostreamer/audiostreamer.dart';

const HOST = "localhost:8000";

void main() {
  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  MyApp({super.key});
  @override
  _MyWidgetState createState() => _MyWidgetState();
}

class _MyWidgetState extends State<MyApp> {
  late WebSocketChannel _channel;
  final _record = Audiostreamer();
  bool _isRecording = false;
  List<dynamic> _inputDevices = [];
  String status = "Initializing...";

  @override
  void initState() {
    super.initState();
    _initialize();
  }

  void _initialize() async {
    try {
      _inputDevices = (await _record.listDevices())["inputs"] ?? [];
      setState(() => status = "Ready");
    } catch (e) {
      setState(() => status = "Initialization failed: $e");
    }
  }

  Future<bool> get isRecording async {
    return await _record.isRecording();
  }

  Future<void> _handleRecordingAction() async {
    try {
      final recording = await isRecording;
      if (recording) {
        await _record.stop();
        setState(() {
          _isRecording = !recording;
          status = "Stopped.";
        });
        _channel.sink.close();
      } else {
        _channel = WebSocketChannel.connect(Uri.parse('ws://$HOST/out_ws'));
        final stream = await _record.start(_inputDevices[0]?["id"]);
        stream.listen(
          (data) => _channel.sink.add(data),
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
              _buildControlButtons(),
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
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          const Padding(
            padding: EdgeInsets.all(8.0),
            child: Text("Input Devices:", style: TextStyle(fontWeight: FontWeight.bold)),
          ),
          SizedBox(
            height: 200, // Set height as needed
            child: ListView.builder(
              itemCount: _inputDevices.length,
              itemBuilder: (context, index) {
                final device = _inputDevices[index];
                return ListTile(
                  title: Text(device['label'] ?? 'Unknown device'),
                  subtitle: Text(device['id'] ?? 'No ID'),
                );
              },
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildControlButtons() {
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

  @override
  void dispose() {
    _record.dispose();
    _channel.sink.close();
    super.dispose();
  }
}
