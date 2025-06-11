# Project-level configuration.
cmake_minimum_required(VERSION 3.14)

# The name of the executable created for the application. Change this to change
# the on-disk name of your application.
set(BINARY_NAME "socket_audiostream")
project(${BINARY_NAME} LANGUAGES CXX)

# Explicitly opt in to modern CMake behaviors to avoid warnings with recent
# versions of CMake.
cmake_policy(VERSION 3.14...3.25)

# Use Unicode for all projects.
add_definitions(-DUNICODE -D_UNICODE)

# Pass -DSTEREO to compiler if environment variable is set
string(STRIP "$ENV{STEREO}" STEREO_VALUE)
if(STEREO_VALUE STREQUAL "1")
  add_definitions(-DSTEREO)
endif()

# Compilation settings that should be applied to most targets.
#
# Be cautious about adding new options here, as plugins use this function by
# default. In most cases, you should add new options to specific targets instead
# of modifying this function.
function(APPLY_STANDARD_SETTINGS TARGET)
  target_compile_features(${TARGET} PUBLIC cxx_std_17)
endfunction()

# Flutter library and tool build rules. (flutter create .)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/flutter") # must contain CMakeLists.txt
  message(STATUS "Compile a build")

  # Flutter library and tool build rules.
  add_subdirectory("flutter")

  # Application build; see runner/CMakeLists.txt.
  add_subdirectory("runner")

  # === Installation ===
  # Support files are copied into place next to the executable, so that it can
  # run in place. This is done instead of making a separate bundle (as on Linux)
  # so that building and running from within Visual Studio will work.
  set(BUILD_BUNDLE_DIR "$<TARGET_FILE_DIR:${BINARY_NAME}>")

  # This block is critical for Flutter desktop builds because Flutter generates a INSTALL.vcxproj
  if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${BUILD_BUNDLE_DIR}" CACHE PATH "..." FORCE)
  endif()

  # Install FILES given no DESTINATION!
  set(INSTALL_BUNDLE_DATA_DIR "${CMAKE_INSTALL_PREFIX}/data")
  set(INSTALL_BUNDLE_LIB_DIR "${CMAKE_INSTALL_PREFIX}")

  # [FATAL:flutter/fml/icu_util.cc(97)] Check failed: context->IsValid(). Must be able to initialize the ICU context
  install(FILES "${FLUTTER_ICU_DATA_FILE}" DESTINATION "${INSTALL_BUNDLE_DATA_DIR}" COMPONENT Runtime)

  # Error waiting for a debug connection
  install(FILES "${FLUTTER_LIBRARY}" DESTINATION "${INSTALL_BUNDLE_LIB_DIR}" COMPONENT Runtime)

  # Fully re-copy the assets directory on each build to avoid having stale files
  # from a previous install.
  set(FLUTTER_ASSET_DIR_NAME "flutter_assets")

  # 'FlutterEngineInitialize' returned 'kInvalidArguments'. Not running in AOT mode
  install(DIRECTORY "${PROJECT_BUILD_DIR}/${FLUTTER_ASSET_DIR_NAME}" DESTINATION "${INSTALL_BUNDLE_DATA_DIR}" COMPONENT Runtime)

else()
  message(STATUS "Compile a custom plugin")

  # Compile Plugin
  set(PLUGIN_NAME "${BINARY_NAME}_plugin")
  project(${PLUGIN_NAME} LANGUAGES CXX)

  # Define the plugin target
  add_library(${PLUGIN_NAME} SHARED
    "include/socket_audiostream/socket_audiostream_plugin.cpp"
    "include/socket_audiostream/recording/mediarecorder.cpp"
    "include/socket_audiostream/recording/recorder.cpp"
    "include/socket_audiostream/playback/mediaplayer.cpp"
    "include/socket_audiostream/playback/player.cpp"
  )

  # Add dependency libraries and include directories.
  target_link_libraries(${PLUGIN_NAME} PRIVATE
    flutter_wrapper_app # flutter::FlutterViewController
    flutter_wrapper_plugin # PluginRegistrar::PluginRegistrar
    mf.lib # MFCreateDeviceSource, MFEnumDeviceSources, 
    mfplat.lib # MFStartup, MFCreateAttributes
    mfreadwrite.lib # MFCreateSourceReaderFromMediaSource
    mfuuid.lib  # MFMediaType_Audio, MFAudioFormat_PCM, MF_MT_SUBTYPE, MF_MT_AUDIO_NUM_CHANNELS etc.
  )

  # 'AudioStreamerPluginRegisterWithRegistrar': inconsistent dll linkage
  target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)

  # Cannot open include file: 'audiostreamer/audio_streamer_plugin.h'
  target_include_directories(${PLUGIN_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
endif()