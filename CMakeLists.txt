cmake_minimum_required(VERSION 3.22)
project(Pulse VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: uses the local copy in tools/JUCE if present (see INSTALL_LOG.md), otherwise downloads it.
include(FetchContent)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/JUCE/CMakeLists.txt" AND NOT FETCHCONTENT_SOURCE_DIR_JUCE)
    set(FETCHCONTENT_SOURCE_DIR_JUCE "${CMAKE_CURRENT_SOURCE_DIR}/tools/JUCE")
endif()
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.15 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# ---- DSP core: shared by the plugin and the offline render/test tool --------------------------
set(PULSE_DSP_SOURCES
    src/Params.cpp
    src/dsp/TapeModule.cpp
    src/dsp/GritModule.cpp
    src/dsp/ShapeModule.cpp
    src/dsp/PunchModule.cpp
    src/dsp/GlobalSection.cpp
    src/engine/PulseEngine.cpp)

# ---- Plugin ------------------------------------------------------------------------------------
juce_add_plugin(Pulse
    COMPANY_NAME "PulseLab"
    PLUGIN_MANUFACTURER_CODE Plsl
    PLUGIN_CODE Pls1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Pulse"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Dynamics Distortion
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Pulse PRIVATE ${PULSE_DSP_SOURCES} src/PluginProcessor.cpp)
target_include_directories(Pulse PRIVATE src)
target_compile_definitions(Pulse PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Pulse
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---- Offline renderer / test tool --------------------------------------------------------------
juce_add_console_app(pulse_render PRODUCT_NAME "pulse_render")
target_sources(pulse_render PRIVATE ${PULSE_DSP_SOURCES} tests/RenderMain.cpp)
target_include_directories(pulse_render PRIVATE src)
target_compile_definitions(pulse_render PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(pulse_render
    PRIVATE juce::juce_audio_formats juce::juce_audio_processors juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
