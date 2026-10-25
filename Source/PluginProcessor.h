#pragma once

#include "Common.h"
#include "License.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

struct Sample
{
    juce::AudioBuffer<float> data;
    double sampleRate = 44100.0;
    juce::String name;
    juce::File file;            // empty for factory samples
    juce::String factoryId;     // original file name of an embedded factory sample

    // Velocity layers: when not empty, this object only describes the pad (name / factoryId) and the audio
    // comes from layers[i] for the first i with velocity <= layerMax[i] (soft -> hard).
    std::vector<std::shared_ptr<const Sample>> layers;
    std::vector<float> layerMax;
    std::shared_ptr<const Sample> half;   // hi-hat half-open sample (used with the CC4 pedal)
    std::array<std::shared_ptr<const Sample>, 4> mics;   // cymbals: Dry / Studio / Room / Hall sets (each with its own layers)
};

struct FactorySampleInfo
{
    juce::String kit;        // kit name
    juce::String name;       // readable sample name
    juce::String resource;   // embedded file name (also the factoryId stored in presets)
    int kitIndex = 0;
    int pad = -1;            // pad this sample belongs to in its kit (0-based)
};

struct KitInfo
{
    juce::String name;
    juce::File folder;          // user kit folder (empty for factory kits)
    int factoryIndex = -1;      // 0..3 for embedded kits, -1 for user kits
};

// MIDI groove library (embedded in the plugin)
struct GrooveSection
{
    juce::String label;       // "Intro", "Verse", ...
    juce::String resource;    // original embedded file name, e.g. Gospel__Chop__01_Intro.mid
};

struct GrooveEntry
{
    juce::String name;
    std::vector<GrooveSection> sections;
};

struct GenreEntry
{
    juce::String name;
    std::vector<GrooveEntry> grooves;
};

struct GrooveSeq
{
    struct Ev { double beat; juce::MidiMessage msg; };
    std::vector<Ev> events;
    double lengthBeats = 4.0;
    double bpm = 120.0;
    juce::String resource;
};

class DrumForgeProcessor : public juce::AudioProcessor,
                           private juce::AsyncUpdater,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    DrumForgeProcessor();
    ~DrumForgeProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "African Drum"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Sample management (message thread)
    bool isSupportedFile (const juce::File&) const;
    bool loadSample (int pad, const juce::File&);
    void clearSample (int pad);
    void clearAllSamples();
    void loadFolder (const juce::File&);
    juce::String getSampleName (int pad) const;
    std::shared_ptr<const Sample> peekSample (int pad) const { return getSampleShared (pad); }   // for tools / tests

    // Kits: 4 embedded factory kits + every sub-folder of the user's Kits folder.
    // A kit can be switched at any time, also while playing.
    static const juce::StringArray& getPadNames();
    static juce::File getKitsFolder();
    void rescanKits();
    std::vector<KitInfo> getKits() const;
    bool loadKit (int index);

    // Every embedded kit sample (mapped ones and spares). Any of them can be swapped onto any pad.
    static std::vector<FactorySampleInfo> getFactorySamples();
    bool loadFactorySample (int pad, const juce::String& resource) { return loadFactoryResource (pad, resource); }
    juce::String getCurrentKitName() const;

    // Cymbals: velocity layers + chosen microphone (Dry / Studio / Room / Hall). Reloaded when the mic changes.
    void reloadCymbals();

    // Mixer presets (saved per kit in Documents/African Gospel/African Drum/Presets/<kit>/)
    static juce::File getPresetsFolder();
    juce::StringArray listMixPresets() const;                 // names of the current kit's presets
    void installFactoryMixPresets() const;
    bool saveMixPreset (const juce::String& name) const;
    bool loadMixPreset (const juce::String& name);
    bool deleteMixPreset (const juce::String& name) const;

    // MIDI grooves (intro, verse, chorus, bridge, break, fills...) played by the plugin itself
    static const std::vector<GenreEntry>& getGrooveLibrary();
    static juce::String getGrooveDisplayName (const juce::String& resource);
    bool startGroove (const juce::String& resource, bool restart = true);
    const GrooveSeq* loadGrooveSeq (const juce::String& resource);   // message thread; pointer stays valid for a long time
    void stopGroove();
    bool isGroovePlaying() const { return groovePlaying.load(); }
    float getGrooveProgress() const { return grooveProgress.load(); }
    double getGrooveBpm() const { return grooveBpm.load(); }
    juce::File writeGrooveFile (const juce::String& resource, const juce::File& folder) const;

    // External MIDI files: copied into Documents/African Gospel/African Drum/MIDI Import and shown as the "My MIDI" genre.
    // gmMap = convert General MIDI drum notes (e.g. 38 snare, 42 hi-hat) to this plugin's pad map.
    static juce::File getUserMidiFolder();
    static std::vector<GenreEntry> scanUserMidi (bool gmMap);
    int importMidiFiles (const juce::Array<juce::File>& filesOrFolders);   // returns number of files copied
    int exportGrooveLibrary (const juce::File& folder) const;
    std::atomic<bool> grooveLoop { true }, grooveSyncHost { true };

    // External MIDI note mapping (e-drum brands, General MIDI, AD2 Standard, custom MIDI learn).
    // Applies to incoming MIDI only; the built-in grooves always use the native map (C1..D#2).
    struct MidiMapMenu
    {
        juce::String label, id;
        std::vector<MidiMapMenu> children;
        bool separator = false;
    };
    static std::vector<MidiMapMenu> getMidiMapMenu();
    static juce::String midiMapLabel (const juce::String& id);
    void setMidiMap (const juce::String& id);
    juce::String getMidiMapId() const;
    bool isMidiMapLearned() const;           // true when MIDI Learn changed the current map
    void clearMidiLearn();
    void startMidiLearn (int pad) { learnPad.store (pad); }
    int getLearnPad() const { return learnPad.load(); }
    std::atomic<int> lastMidiNote { -1 };    // for the on-screen note display

    // Licence (only enforced when built with AG_REQUIRE_LICENSE=ON; otherwise always licensed)
    static constexpr bool licenseRequired() { return DF_REQUIRE_LICENSE != 0; }
    bool isLicensed() const { return ! licenseRequired() || licensed.load(); }
    juce::String getLicenseHolder() const { const juce::ScopedLock sl (kitLock); return licenseName; }
    bool activateLicense (const juce::String& name, const juce::String& email, const juce::String& key);

    // Preview from the GUI (thread safe)
    void triggerPad (int pad, float velocity = 0.8f);

    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<float>, df::numPads> padLevel;
    std::atomic<float> masterPeakL { 0.0f }, masterPeakR { 0.0f };
    std::atomic<float> compReduction { 0.0f };   // dB, for the GR meter
    juce::File lastDirectory;

private:
    void parameterChanged (const juce::String&, float) override {}
    void handleAsyncUpdate() override { reloadCymbals(); }
    bool loadCymbalSet (int pad);
    std::shared_ptr<Sample> decodeFactory (const juce::String& original, const juce::String& name);
    void publishSample (int pad, std::shared_ptr<const Sample>);
    std::atomic<bool> licensed { false };
    juce::String licenseName;
    double demoSamples = 0.0;
    std::atomic<int> hatCC { -1 };           // last CC4 (foot controller) value, -1 = never received
    static BusesProperties makeBuses();

    struct PadSlot
    {
        juce::SpinLock lock;
        std::shared_ptr<const Sample> sample;
    };

    struct Voice
    {
        std::shared_ptr<const Sample> sample;
        int pad = -1;
        double pos = 0.0;
        double inc = 1.0;
        float vel = 1.0f;
        float env = 1.0f;
        float relStep = 0.0f;
        float extra = 1.0f;         // per-voice gain (microphone level)
        float tone = 1.0f;          // velocity-controlled low-pass coefficient (1 = open)
        float lpL = 0.0f, lpR = 0.0f;
        bool  toneOn = false;
        float decayMul = 1.0f;      // per-sample decay multiplier (1 = no decay shaping)
        bool active = false;
        bool releasing = false;
        juce::uint64 order = 0;
    };

    struct PadRT
    {
        float gainL = 0.0f, gainR = 0.0f;
        int bus = 0;
    };

    std::shared_ptr<const Sample> getSampleShared (int pad) const;
    void pruneKeepAlive();

    bool loadSampleInternal (int pad, const juce::File&);
    bool installSample (int pad, std::unique_ptr<juce::AudioFormatReader>, const juce::String& name,
                        const juce::File&, const juce::String& factoryId);
    bool loadFactoryResource (int pad, const juce::String& fileNamePrefix);
    juce::Array<juce::File> audioFilesIn (const juce::File& folder) const;
    void setCurrentKitName (const juce::String&);

    mutable juce::CriticalSection kitLock;
    std::vector<KitInfo> kits;
    juce::String currentKitName;

    void updateBlockState (juce::AudioBuffer<float>&);
    void render (int start, int num);
    void handleMidi (const juce::MidiMessage&);
    void startVoice (int pad, float velocity);
    void spawnVoice (int pad, std::shared_ptr<const Sample> s, float velocity, float extra, bool tone);
    void chokeFor (int pad);
    juce::AudioParameterFloat* micP[4] = {};
    std::atomic<float>* sensP = nullptr;
    float micLevel (int m) const { return micP[m] != nullptr ? micP[m]->get() : 0.0f; }
    void releaseVoice (Voice&, double seconds);

    mutable std::array<PadSlot, df::numPads> slots;
    std::vector<std::shared_ptr<const Sample>> keepAlive;
    juce::CriticalSection loadLock;
    juce::AudioFormatManager formatManager;

    std::array<Voice, df::maxVoices> voices;
    juce::uint64 voiceCounter = 0;
    double currentSampleRate = 44100.0;

    std::array<PadRT, df::numPads> padRT;
    std::array<float, df::numPads> blockPeak {};
    std::array<float*, df::numOutputs> outL {}, outR {};
    std::array<std::atomic<int>, df::numPads> pendingTrigger;

    std::atomic<float>* masterP = nullptr;
    std::atomic<float>* roomSizeP = nullptr;
    std::atomic<float>* roomMixP = nullptr;
    std::atomic<float>* humanizeP = nullptr;
    std::array<std::atomic<float>*, df::numPads> gainP {}, panP {}, pitchP {}, muteP {},
                                                 soloP {}, outP {}, chokeP {}, gateP {}, decayP {};

    // MIDI mapping
    void remapExternalMidi (juce::MidiBuffer&);
    std::array<std::atomic<int>, 128> noteMap, learnedPad;   // note -> pad (-1 = ignored), learned overrides
    std::atomic<int> learnPad { -1 };
    juce::String midiMapId { "native" };
    mutable juce::CriticalSection mapLock;

    juce::Reverb reverb;

    // master FX chain (Main bus): EQ -> compressor -> saturation -> delay -> reverb -> width
    struct FxParams
    {
        std::atomic<float> *eqOn {}, *eqLow {}, *eqMid {}, *eqFreq {}, *eqHigh {},
                           *compOn {}, *compThr {}, *compRatio {}, *compAtt {}, *compRel {}, *compMakeup {},
                           *satOn {}, *satDrive {}, *satMix {},
                           *dlyOn {}, *dlyTime {}, *dlyFb {}, *dlyMix {},
                           *roomDamp {}, *width {};
    } fx;

    void applyMainFx (int numSamples);
    juce::IIRFilter eqLowF[2], eqMidF[2], eqHighF[2];
    float eqCache[4] = { 1e9f, 1e9f, 1e9f, 1e9f };
    float compEnv = 0.0f;
    std::vector<float> dlyL, dlyR;
    int dlyPos = 0;
    float dlyLpL = 0.0f, dlyLpR = 0.0f;
    juce::Random rng;

    // groove playback
    void generateGrooveMidi (juce::MidiBuffer&, int numSamples);
    std::vector<std::unique_ptr<GrooveSeq>> seqKeep;   // message thread only, never shrinks below 32
    std::atomic<const GrooveSeq*> activeSeq { nullptr };
    std::atomic<bool> groovePlaying { false }, grooveReset { false };
    std::atomic<float> grooveProgress { 0.0f };
    std::atomic<double> grooveBpm { 120.0 };
    double groovePos = 0.0;

    void assignFilesToPads (const juce::Array<juce::File>&, std::array<juce::File, df::numPads>&) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumForgeProcessor)
};
