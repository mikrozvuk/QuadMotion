cmake_minimum_required(VERSION 3.22)
project(QuadMotion VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is downloaded automatically on the first configure (needs git + internet once).
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(QuadMotion
    COMPANY_NAME             "MyStudio"          # change to your own name
    PLUGIN_MANUFACTURER_CODE Mstd                # 4 chars, first one uppercase
    PLUGIN_CODE              Qmot                # 4 chars, unique per plugin
    FORMATS                  VST3
    PRODUCT_NAME             "QuadMotion"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    VST3_CATEGORIES          Fx Spatial
    COPY_PLUGIN_AFTER_BUILD  FALSE)

target_sources(QuadMotion PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_definitions(QuadMotion PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(QuadMotion
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
