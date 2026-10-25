cmake_minimum_required(VERSION 3.22)

# --- Compatibility (must be set before project()) ---------------------------------------
# macOS: universal binary (Apple Silicon + Intel), deployment target as low as JUCE 8 allows
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
# Windows: link the C++ runtime statically so users do NOT need the Visual C++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(DrumForge VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DRUMFORGE_COPY_AFTER_BUILD "Copy the plugin to the system plugin folder after build" OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(DRUMFORGE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND DRUMFORGE_FORMATS AU)
endif()

juce_add_plugin(DrumForge
    COMPANY_NAME "African Gospel"
    PRODUCT_NAME "African Drum"
    PLUGIN_NAME "African Drum"
    FORMATS ${DRUMFORGE_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Agsp
    PLUGIN_CODE Agd1
    VST3_CATEGORIES Instrument Drum Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD ${DRUMFORGE_COPY_AFTER_BUILD})

# Embeds the logo, the MIDI (no drum samples are shipped) groove library into the plugin
file(GLOB_RECURSE DRUMFORGE_MIDI_FILES CONFIGURE_DEPENDS Resources/MIDI/*.mid)
file(GLOB DRUMFORGE_KIT_FILES CONFIGURE_DEPENDS Resources/Kits/*.flac)
juce_add_binary_data(DrumForgeAssets
    SOURCES Resources/logo_ag.png Resources/logo_dbl.png Resources/logo_skm.png Resources/kit_scene.png ${DRUMFORGE_MIDI_FILES} ${DRUMFORGE_KIT_FILES})

target_sources(DrumForge PRIVATE
    Source/Common.h
    Source/LookAndFeel.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)

option(AG_REQUIRE_LICENSE "Require a licence key (demo mode mutes audio periodically when unlicensed)" OFF)
set(AG_LICENSE_SECRET "CHANGE-THIS-SECRET-BEFORE-RELEASE" CACHE STRING "Secret used to validate licence keys")

target_compile_definitions(DrumForge PUBLIC
    DF_REQUIRE_LICENSE=$<BOOL:${AG_REQUIRE_LICENSE}>
    DF_LICENSE_SECRET=\"${AG_LICENSE_SECRET}\"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(DrumForge
    PRIVATE
        DrumForgeAssets
        juce::juce_audio_utils
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
