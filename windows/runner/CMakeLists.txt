cmake_minimum_required(VERSION 3.14)

# Debug message
message(STATUS "CMAKE_SOURCE_DIR is: ${CMAKE_SOURCE_DIR}")
message(STATUS "CMAKE_CURRENT_SOURCE_DIR is: ${CMAKE_CURRENT_SOURCE_DIR}")

# Define the application target. To change its name, change BINARY_NAME in the
# top-level CMakeLists.txt, not the value here, or `flutter run` will no longer
# work.
#
# Any new source files that you add to the application should be added here.
add_executable(${BINARY_NAME} WIN32
  "flutter_window.cpp"
  "main.cpp"
  "utils.cpp"
  "win32_window.cpp"
  "../flutter/generated_plugin_registrant.cc"
  "../include/audiostreamer/audio_streamer_plugin.cpp"
  "../include/audiostreamer/recording/audiostreamer.cpp"
  "../include/audiostreamer/recording/recorder.cpp"
)

# Add preprocessor definitions for the build version.
target_compile_definitions(${BINARY_NAME} PRIVATE "FLUTTER_VERSION=\"${FLUTTER_VERSION}\"")
target_compile_definitions(${BINARY_NAME} PRIVATE "FLUTTER_VERSION_MAJOR=${FLUTTER_VERSION_MAJOR}")
target_compile_definitions(${BINARY_NAME} PRIVATE "FLUTTER_VERSION_MINOR=${FLUTTER_VERSION_MINOR}")
target_compile_definitions(${BINARY_NAME} PRIVATE "FLUTTER_VERSION_PATCH=${FLUTTER_VERSION_PATCH}")
target_compile_definitions(${BINARY_NAME} PRIVATE "FLUTTER_VERSION_BUILD=${FLUTTER_VERSION_BUILD}")

# Disable Windows macros that collide with C++ standard library functions.
target_compile_definitions(${BINARY_NAME} PRIVATE "NOMINMAX")

# Add dependency libraries and include directories.
target_link_libraries(${BINARY_NAME} PRIVATE
  flutter_wrapper_app # flutter::FlutterViewController
  flutter_wrapper_plugin # PluginRegistrar::PluginRegistrar
  mf.lib # MFCreateDeviceSource, MFEnumDeviceSources, 
  mfplat.lib # MFStartup, MFCreateAttributes
  mfreadwrite.lib # MFCreateSourceReaderFromMediaSource
  mfuuid.lib  # MFMediaType_Audio, MFAudioFormat_PCM, MF_MT_SUBTYPE, MF_MT_AUDIO_NUM_CHANNELS etc.
  dwmapi.lib  # win32_window.cpp UpdateTheme
)

# 'AudioStreamerPluginRegisterWithRegistrar': inconsistent dll linkage
target_compile_definitions(${BINARY_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)

# Cannot open include file: 'audiostreamer/audio_streamer_plugin.h'
target_include_directories(${BINARY_NAME} PRIVATE "${CMAKE_SOURCE_DIR}")

# Run the Flutter tool portions of the build. This must not be removed.
add_dependencies(${BINARY_NAME} flutter_assemble)