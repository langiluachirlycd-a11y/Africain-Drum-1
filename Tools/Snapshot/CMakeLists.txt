# Developer tool (not part of the plugin): renders the real plugin editor to PNG frames + audio.
cmake_minimum_required(VERSION 3.22)
project(AgSnapshot VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(DF ${CMAKE_CURRENT_LIST_DIR}/../..)

add_subdirectory(${JUCE_DIR} juce)

file(GLOB_RECURSE MIDIS ${DF}/Resources/MIDI/*.mid)
file(GLOB KITFLACS ${DF}/Resources/Kits/*.flac)
juce_add_binary_data(Assets SOURCES ${DF}/Resources/logo_ag.png ${DF}/Resources/logo_dbl.png ${DF}/Resources/logo_skm.png ${DF}/Resources/kit_scene.png ${MIDIS} ${KITFLACS})

juce_add_console_app(Snapshot PRODUCT_NAME "Snapshot")
target_sources(Snapshot PRIVATE main.cpp ${DF}/Source/PluginProcessor.cpp ${DF}/Source/PluginEditor.cpp)
target_include_directories(Snapshot PRIVATE ${DF}/Source)
target_compile_definitions(Snapshot PRIVATE DF_DEV_ALLOW_EXTERNAL_SAMPLES=1
    JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0
    JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(Snapshot PRIVATE Assets juce::juce_audio_utils juce::juce_cryptography
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
