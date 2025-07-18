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
  "${CMAKE_SOURCE_DIR}/flutter/generated_plugin_registrant.cc"
  "${CMAKE_SOURCE_DIR}/include/socket_audiostream/socket_audiostream_plugin.cpp"
  "${CMAKE_SOURCE_DIR}/include/socket_audiostream/recording/mediarecorder.cpp"
  "${CMAKE_SOURCE_DIR}/include/socket_audiostream/recording/recorder.cpp"
  "${CMAKE_SOURCE_DIR}/include/socket_audiostream/playback/mediaplayer.cpp"
  "${CMAKE_SOURCE_DIR}/include/socket_audiostream/playback/player.cpp"
)

# Collect all denoise .c files into a variable
file(GLOB DENOISE_SOURCES "${CMAKE_SOURCE_DIR}/denoise/*.c")

# Add them to the build target
target_sources(${BINARY_NAME} PRIVATE ${DENOISE_SOURCES})

# Ensure they're compiled as C (not C++)
set_source_files_properties(${DENOISE_SOURCES} PROPERTIES LANGUAGE C)

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

# Cannot open include file: 'audiostreamer/audio_streamer_plugin.h' and denoise.h
target_include_directories(${BINARY_NAME} 
  PRIVATE "${CMAKE_SOURCE_DIR}"
  PRIVATE "${CMAKE_SOURCE_DIR}/denoise"
)

# Run the Flutter tool portions of the build. This must not be removed.
add_dependencies(${BINARY_NAME} flutter_assemble)