cmake_minimum_required(VERSION 3.22)

# Loopsmith DAW plugin: your Loopsmith studio inside your DAW (AU + VST3).
# macOS builds are universal (Intel + Apple Silicon) and run on macOS 10.15 or newer.
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Oldest macOS the plugin runs on")
  if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build for Intel and Apple Silicon")
  endif()
endif()

project(LoopsmithPlugin VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
if(DEFINED JUCE_DIR)
  add_subdirectory(${JUCE_DIR} JUCE)
else()
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

set(LOOPSMITH_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND LOOPSMITH_FORMATS AU)
endif()

juce_add_plugin(Loopsmith
  COMPANY_NAME "Loopsmith"
  COMPANY_WEBSITE ""
  BUNDLE_ID "com.loopsmith.plugin"
  PLUGIN_MANUFACTURER_CODE Lpsm
  PLUGIN_CODE Lsm1
  PRODUCT_NAME "Loopsmith"
  DESCRIPTION "Your Loopsmith beat and sample studio inside your DAW"
  FORMATS ${LOOPSMITH_FORMATS}
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  NEEDS_WEB_BROWSER TRUE
  NEEDS_CURL TRUE
  AU_MAIN_TYPE kAudioUnitType_Effect
  VST3_CATEGORIES Fx Tools
  COPY_PLUGIN_AFTER_BUILD FALSE
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Loopsmith's standalone app can capture audio to send to your studio.")

target_sources(Loopsmith PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/StudioClient.cpp)

target_compile_definitions(Loopsmith PUBLIC
  JUCE_WEB_BROWSER=1
  $<$<AND:$<BOOL:${UNIX}>,$<NOT:$<BOOL:${APPLE}>>>:JUCE_USE_CURL=1>
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0)

target_link_libraries(Loopsmith
  PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
