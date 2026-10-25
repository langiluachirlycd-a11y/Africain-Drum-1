#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

namespace df
{
    // false = the plugin refuses to read any sample from the user's disk (drag & drop, LOAD, folders, kits, saved paths)
#ifdef DF_DEV_ALLOW_EXTERNAL_SAMPLES   // only the offline screenshot/demo tool defines this
    constexpr bool externalSamplesEnabled = true;
#else
    constexpr bool externalSamplesEnabled = false;
#endif
    constexpr int numPads        = 21;
    constexpr int numOutputs     = 16;   // 16 stereo output buses (Main + 15 extra)
    constexpr int firstNote      = 36;   // C1 -> pad 1 ... G#3 (56) -> pad 21
    constexpr int maxVoices      = 64;
    constexpr int numChokeGroups = 8;

    inline juce::String pid (const juce::String& base, int pad)
    {
        return base + juce::String (pad + 1);
    }
}
