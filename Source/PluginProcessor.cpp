#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

using namespace juce;

namespace
{
    // Kits k1..k3 hold kick/snare/side-stick/toms; the cymbals are shared by every kit (files "kc_NN_...")
    const char* const factoryKitNames[] = { "Standard Kit", "Soukous Fusion", "Seben Kit" };
    constexpr int numFactoryKits = 3;
    constexpr int firstSharedPad = 9;     // pads 10..21 (index 9..20) come from the shared cymbal set

    // "k5_01_Kick_C1.flac" -> kit 5, pad 1, "Kick C1"; spares are "k5_x03_Name.flac"
    bool parseFactoryName (const String& original, int& kit, bool& spare, int& number, String& readable)
    {
        if (original.length() < 6 || original[0] != 'k' || original[2] != '_')
            return false;

        const bool shared = original[1] == 'c';
        if (! shared && ! CharacterFunctions::isDigit (original[1]))
            return false;

        kit = shared ? 0 : original.substring (1, 2).getIntValue();
        const auto rest = original.substring (3);
        spare = rest.startsWithChar ('x');
        number = rest.substring (spare ? 1 : 0, spare ? 3 : 2).getIntValue();
        readable = rest.fromFirstOccurrenceOf ("_", false, false).upToLastOccurrenceOf (".", false, false).replaceCharacter ('_', ' ');
        return true;
    }
}

namespace
{
    AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "master", 1 }, "Master",
            NormalisableRange<float> (-60.0f, 6.0f, 0.1f), -6.0f,   // 6 dB of headroom by default
            AudioParameterFloatAttributes().withLabel ("dB")));

        {
            const char* ids[] = { "micDry", "micStudio", "micRoom", "micHall" };
            const char* nm[]  = { "Mic Dry", "Mic Studio", "Mic Room", "Mic Hall" };
            const float def[] = { 0.0f, 0.85f, 0.0f, 0.0f };
            for (int m = 0; m < 4; ++m)
                layout.add (std::make_unique<AudioParameterFloat> (
                    ParameterID { ids[m], 1 }, nm[m], NormalisableRange<float> (0.0f, 1.0f, 0.001f), def[m]));
        }

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "sens", 1 }, "Sensitivity", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.6f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "roomSize", 1 }, "Room Size",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.45f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "roomMix", 1 }, "Room Mix",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "humanize", 1 }, "Humanize",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

        auto addF = [&layout] (const char* id, const char* name, float lo, float hi, float step, float def, const char* unit = "", float skew = 1.0f)
        {
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                NormalisableRange<float> (lo, hi, step, skew), def, AudioParameterFloatAttributes().withLabel (unit)));
        };
        auto addB = [&layout] (const char* id, const char* name, bool def)
        {
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
        };

        addB ("eqOn", "EQ On", false);
        addF ("eqLow", "EQ Low", -12.0f, 12.0f, 0.1f, 0.0f, "dB");
        addF ("eqMid", "EQ Mid", -12.0f, 12.0f, 0.1f, 0.0f, "dB");
        addF ("eqFreq", "EQ Mid Freq", 150.0f, 6000.0f, 1.0f, 1000.0f, "Hz", 0.4f);
        addF ("eqHigh", "EQ High", -12.0f, 12.0f, 0.1f, 0.0f, "dB");
        addB ("compOn", "Comp On", false);
        addF ("compThr", "Comp Threshold", -40.0f, 0.0f, 0.1f, -14.0f, "dB");
        addF ("compRatio", "Comp Ratio", 1.0f, 20.0f, 0.1f, 4.0f, ":1", 0.5f);
        addF ("compAtt", "Comp Attack", 0.1f, 80.0f, 0.1f, 8.0f, "ms", 0.4f);
        addF ("compRel", "Comp Release", 20.0f, 800.0f, 1.0f, 140.0f, "ms", 0.5f);
        addF ("compMakeup", "Comp Makeup", 0.0f, 18.0f, 0.1f, 3.0f, "dB");
        addB ("satOn", "Saturation On", false);
        addF ("satDrive", "Saturation Drive", 0.0f, 1.0f, 0.001f, 0.35f);
        addF ("satMix", "Saturation Mix", 0.0f, 1.0f, 0.001f, 0.6f);
        addB ("dlyOn", "Delay On", false);
        addF ("dlyTime", "Delay Time", 20.0f, 1000.0f, 1.0f, 280.0f, "ms", 0.5f);
        addF ("dlyFb", "Delay Feedback", 0.0f, 0.9f, 0.001f, 0.35f);
        addF ("dlyMix", "Delay Mix", 0.0f, 1.0f, 0.001f, 0.25f);
        addF ("roomDamp", "Room Damping", 0.0f, 1.0f, 0.001f, 0.5f);
        addF ("width", "Stereo Width", 0.0f, 2.0f, 0.001f, 1.0f);

        StringArray outChoices, chokeChoices;
        for (int o = 0; o < df::numOutputs; ++o)
            outChoices.add (o == 0 ? String ("Main") : "Out " + String (o + 1));

        chokeChoices.add ("No choke");
        for (int c = 1; c <= df::numChokeGroups; ++c)
            chokeChoices.add ("Choke " + String (c));

        for (int i = 0; i < df::numPads; ++i)
        {
            const String n = "Pad " + String (i + 1) + " ";

            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { df::pid ("gain", i), 1 }, n + "Level",
                NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f,
                AudioParameterFloatAttributes().withLabel ("dB")));

            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { df::pid ("pan", i), 1 }, n + "Pan",
                NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { df::pid ("pitch", i), 1 }, n + "Pitch",
                NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
                AudioParameterFloatAttributes().withLabel ("st")));

            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { df::pid ("decay", i), 1 }, n + "Decay",
                NormalisableRange<float> (0.02f, 5.0f, 0.001f, 0.35f), 5.0f,
                AudioParameterFloatAttributes().withLabel ("s")));

            layout.add (std::make_unique<AudioParameterBool> (
                ParameterID { df::pid ("mute", i), 1 }, n + "Mute", false));

            layout.add (std::make_unique<AudioParameterBool> (
                ParameterID { df::pid ("solo", i), 1 }, n + "Solo", false));

            layout.add (std::make_unique<AudioParameterBool> (
                ParameterID { df::pid ("gate", i), 1 }, n + "Gate", false));

            layout.add (std::make_unique<AudioParameterChoice> (
                ParameterID { df::pid ("out", i), 1 }, n + "Output", outChoices, 0));

            layout.add (std::make_unique<AudioParameterChoice> (
                ParameterID { df::pid ("choke", i), 1 }, n + "Choke", chokeChoices,
                (i >= 9 && i <= 11) ? 1 : 0));   // hi-hats choke each other by default
        }

        return layout;
    }
}

//==============================================================================
AudioProcessor::BusesProperties DrumForgeProcessor::makeBuses()
{
    BusesProperties p;
    for (int i = 0; i < df::numOutputs; ++i)
        p = p.withOutput (i == 0 ? String ("Main") : "Out " + String (i + 1),
                          AudioChannelSet::stereo(), i == 0);
    return p;
}

DrumForgeProcessor::~DrumForgeProcessor()
{
    cancelPendingUpdate();
}

DrumForgeProcessor::DrumForgeProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    formatManager.registerBasicFormats();
    {
        const char* ids[] = { "micDry", "micStudio", "micRoom", "micHall" };
        for (int m = 0; m < 4; ++m)
            micP[m] = dynamic_cast<AudioParameterFloat*> (apvts.getParameter (ids[m]));
        sensP = apvts.getRawParameterValue ("sens");
    }

    {
        const auto lic = df::License::load();
        licensed.store (lic.valid);
        licenseName = lic.valid ? lic.name : String();
    }

    masterP = apvts.getRawParameterValue ("master");
    roomSizeP = apvts.getRawParameterValue ("roomSize");
    roomMixP = apvts.getRawParameterValue ("roomMix");
    humanizeP = apvts.getRawParameterValue ("humanize");

    fx.eqOn = apvts.getRawParameterValue ("eqOn");       fx.eqLow = apvts.getRawParameterValue ("eqLow");
    fx.eqMid = apvts.getRawParameterValue ("eqMid");     fx.eqFreq = apvts.getRawParameterValue ("eqFreq");
    fx.eqHigh = apvts.getRawParameterValue ("eqHigh");   fx.compOn = apvts.getRawParameterValue ("compOn");
    fx.compThr = apvts.getRawParameterValue ("compThr"); fx.compRatio = apvts.getRawParameterValue ("compRatio");
    fx.compAtt = apvts.getRawParameterValue ("compAtt"); fx.compRel = apvts.getRawParameterValue ("compRel");
    fx.compMakeup = apvts.getRawParameterValue ("compMakeup");
    fx.satOn = apvts.getRawParameterValue ("satOn");     fx.satDrive = apvts.getRawParameterValue ("satDrive");
    fx.satMix = apvts.getRawParameterValue ("satMix");   fx.dlyOn = apvts.getRawParameterValue ("dlyOn");
    fx.dlyTime = apvts.getRawParameterValue ("dlyTime"); fx.dlyFb = apvts.getRawParameterValue ("dlyFb");
    fx.dlyMix = apvts.getRawParameterValue ("dlyMix");   fx.roomDamp = apvts.getRawParameterValue ("roomDamp");
    fx.width = apvts.getRawParameterValue ("width");

    for (int i = 0; i < df::numPads; ++i)
    {
        decayP[(size_t) i] = apvts.getRawParameterValue (df::pid ("decay", i));
        gainP[(size_t) i]  = apvts.getRawParameterValue (df::pid ("gain", i));
        panP[(size_t) i]   = apvts.getRawParameterValue (df::pid ("pan", i));
        pitchP[(size_t) i] = apvts.getRawParameterValue (df::pid ("pitch", i));
        muteP[(size_t) i]  = apvts.getRawParameterValue (df::pid ("mute", i));
        soloP[(size_t) i]  = apvts.getRawParameterValue (df::pid ("solo", i));
        gateP[(size_t) i]  = apvts.getRawParameterValue (df::pid ("gate", i));
        outP[(size_t) i]   = apvts.getRawParameterValue (df::pid ("out", i));
        chokeP[(size_t) i] = apvts.getRawParameterValue (df::pid ("choke", i));

        padLevel[(size_t) i].store (0.0f);
        pendingTrigger[(size_t) i].store (0);
    }

    lastDirectory = File::getSpecialLocation (File::userMusicDirectory);

    setMidiMap ("gm");
    rescanKits();
    loadKit (0);   // no factory kits: pads start empty
}

//==============================================================================
void DrumForgeProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    reverb.setSampleRate (currentSampleRate);
    reverb.reset();

    dlyL.assign ((size_t) (currentSampleRate * 1.1) + 8, 0.0f);
    dlyR.assign (dlyL.size(), 0.0f);
    dlyPos = 0;
    dlyLpL = dlyLpR = 0.0f;
    compEnv = 0.0f;
    for (auto* f : { &eqLowF[0], &eqLowF[1], &eqMidF[0], &eqMidF[1], &eqHighF[0], &eqHighF[1] })
        f->reset();
    for (auto& c : eqCache)
        c = 1e9f;

    for (auto& v : voices)
    {
        v.active = false;
        v.sample.reset();
    }
}

bool DrumForgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto& s = layouts.outputBuses.getReference (i);
        if (! s.isDisabled() && s != AudioChannelSet::stereo())
            return false;
    }

    return true;
}

//==============================================================================
void DrumForgeProcessor::updateBlockState (AudioBuffer<float>& buffer)
{
    // Output bus pointers
    for (int b = 0; b < df::numOutputs; ++b)
    {
        outL[(size_t) b] = nullptr;
        outR[(size_t) b] = nullptr;

        if (auto* bus = getBus (false, b))
        {
            if (bus->isEnabled() && bus->getNumberOfChannels() >= 2)
            {
                const int off = getChannelIndexInProcessBlockBuffer (false, b, 0);
                if (off >= 0 && off + 1 < buffer.getNumChannels())
                {
                    outL[(size_t) b] = buffer.getWritePointer (off);
                    outR[(size_t) b] = buffer.getWritePointer (off + 1);
                }
            }
        }
    }

    // Safety: main bus must exist
    if (outL[0] == nullptr && buffer.getNumChannels() >= 2)
    {
        outL[0] = buffer.getWritePointer (0);
        outR[0] = buffer.getWritePointer (1);
    }

    // Mixer state
    bool anySolo = false;
    for (int i = 0; i < df::numPads; ++i)
        if (soloP[(size_t) i]->load() > 0.5f)
            anySolo = true;

    const float master = Decibels::decibelsToGain (masterP->load(), -60.0f);

    for (int i = 0; i < df::numPads; ++i)
    {
        const auto idx = (size_t) i;
        const bool audible = muteP[idx]->load() < 0.5f
                          && (! anySolo || soloP[idx]->load() > 0.5f);

        const float g   = audible ? Decibels::decibelsToGain (gainP[idx]->load(), -60.0f) * master : 0.0f;
        const float pan = jlimit (-1.0f, 1.0f, panP[idx]->load());

        // Balance law: unity at centre, attenuate the opposite side
        padRT[idx].gainL = g * (pan <= 0.0f ? 1.0f : 1.0f - pan);
        padRT[idx].gainR = g * (pan >= 0.0f ? 1.0f : 1.0f + pan);

        int bus = jlimit (0, df::numOutputs - 1, (int) std::lround (outP[idx]->load()));
        if (outL[(size_t) bus] == nullptr)
            bus = 0;
        padRT[idx].bus = bus;

        blockPeak[idx] = 0.0f;
    }
}


//==============================================================================
// External MIDI maps. Pads: 0 kick, 1 kick2, 2 snare, 3 rim, 4 side stick, 5 floor tom, 6 low tom, 7 mid tom,
// 8 high tom, 9 hat closed, 10 hat pedal, 11 hat open, 12 crash 1, 13 splash (crash 2), 14 ride, 15 ride bell.
namespace
{
    int mapGeneralMidi (int n)
    {
        switch (n)
        {
            case 35: case 36: return 0;
            case 37: return 4;
            case 38: case 40: return 2;
            case 39: return 3;
            case 41: case 43: return 5;
            case 45: return 6;
            case 47: return 7;
            case 48: case 50: return 8;
            case 42: return 9;
            case 44: return 10;
            case 46: return 11;
            case 49: return 12;
            case 57: return 16;
            case 58: return 17;
            case 55: return 13;
            case 52: return 19;
            case 54: return 20;
            case 51: case 59: return 14;
            case 53: case 56: return 15;
            default: return -1;
        }
    }

    // "Standard e-kit" layout used by Roland TD, AD2 Standard and most modules: separate rim shot, T1..T4 = 48/45/43/41
    int mapStandard (int n)
    {
        switch (n)
        {
            case 35: case 36: return 0;
            case 37: return 4;
            case 38: return 2;
            case 39: case 40: return 3;
            case 41: return 5;
            case 43: return 6;
            case 45: return 7;
            case 47: return 7;
            case 48: case 50: return 8;
            case 42: case 22: return 9;
            case 44: return 10;
            case 46: case 26: return 11;
            case 49: return 12;
            case 57: return 16;
            case 58: return 17;
            case 55: return 13;
            case 52: return 19;
            case 54: return 20;
            case 51: case 59: case 60: return 14;
            case 53: case 56: return 15;
            default: return -1;
        }
    }

    // id = "gm", "std", "native", "empty" or "<group>/<model>" -> which layout the model uses
    int layoutFor (const String& id)
    {
        if (id == "empty")  return -1;
        if (id == "native") return 0;
        if (id == "gm")     return 1;
        if (id == "std")    return 2;
        const auto group = id.upToFirstOccurrenceOf ("/", false, false);
        if (group == "Roland" || group == "Alesis" || group == "Yamaha" || group == "Millenium" || group == "Nux" || group == "Efnote" || group == "GEWA")
            return 2;
        if (group == "Other plugins")
            return id.contains ("EZdrummer") || id.contains ("Superior") ? 2 : 1;
        return 1;   // 2box, MegaDRUM, Simmons: General MIDI based
    }
}

std::vector<DrumForgeProcessor::MidiMapMenu> DrumForgeProcessor::getMidiMapMenu()
{
    std::vector<MidiMapMenu> m;
    auto item  = [] (const String& l, const String& id) { MidiMapMenu x; x.label = l; x.id = id; return x; };
    auto sep   = [] { MidiMapMenu x; x.separator = true; return x; };
    auto group = [&] (const String& name, std::initializer_list<const char*> models)
    {
        MidiMapMenu g; g.label = name;
        for (auto* mdl : models) g.children.push_back (item (mdl, name + "/" + mdl));
        return g;
    };

    m.push_back (item ("- Empty -", "empty"));
    m.push_back (item ("AD2 Standard", "std"));
    m.push_back (item ("General MIDI", "gm"));
    m.push_back (item ("African Drum Default (C1 - G#3)", "native"));
    m.push_back (sep());
    m.push_back (group ("2box", { "DrumIt Five", "DrumIt Three", "Drum Module" }));
    m.push_back (group ("Alesis", { "Nitro Mesh", "Strike Pro", "Surge", "DM10", "Crimson II" }));
    m.push_back (group ("Efnote", { "Efnote 3", "Efnote 5", "Efnote 7", "Efnote 7X" }));
    m.push_back (group ("GEWA", { "G3", "G5", "G9" }));
    m.push_back (group ("MegaDRUM", { "Standard", "General MIDI" }));
    m.push_back (group ("Millenium", { "MPS-250", "MPS-450", "MPS-750X" }));
    m.push_back (group ("Nux", { "DM-210", "DM-7X", "DM-8" }));
    m.push_back (group ("Other plugins", { "Superior Drummer 3", "EZdrummer 3", "BFD3", "Addictive Drums 2", "Steven Slate Drums",
                                           "GetGood Drums", "MT Power Drum Kit", "Jamstix" }));
    m.push_back (group ("Roland", { "TD-1", "TD-02", "TD-07", "TD-11", "TD-17", "TD-25", "TD-27", "TD-30", "TD-50", "TD-50X" }));
    m.push_back (group ("Simmons", { "DA2008", "SD600", "SD1000", "SD2000" }));
    m.push_back (group ("Yamaha", { "DTX402", "DTX430K", "DTX450K", "DTX6", "DTX8", "DTX10", "DTX-PRO" }));
    return m;
}

String DrumForgeProcessor::midiMapLabel (const String& id)
{
    if (id == "empty") return "- Empty -";
    if (id == "std") return "AD2 Standard";
    if (id == "gm") return "General MIDI";
    if (id == "native") return "African Drum Default";
    return id.fromFirstOccurrenceOf ("/", false, false).isEmpty() ? id : id.replace ("/", " ");
}

void DrumForgeProcessor::setMidiMap (const String& id)
{
    const int layout = layoutFor (id);

    for (int n = 0; n < 128; ++n)
    {
        int pad = -1;
        if (layout == 0)      pad = isPositiveAndBelow (n - df::firstNote, df::numPads) ? n - df::firstNote : -1;
        else if (layout == 1) pad = mapGeneralMidi (n);
        else if (layout == 2) pad = mapStandard (n);

        noteMap[(size_t) n].store (pad);
        learnedPad[(size_t) n].store (-1);
    }

    const ScopedLock sl (mapLock);
    midiMapId = id;
}

String DrumForgeProcessor::getMidiMapId() const
{
    const ScopedLock sl (mapLock);
    return midiMapId;
}

bool DrumForgeProcessor::isMidiMapLearned() const
{
    for (auto& l : learnedPad)
        if (l.load() >= 0)
            return true;
    return false;
}

void DrumForgeProcessor::clearMidiLearn()
{
    setMidiMap (getMidiMapId());
}

void DrumForgeProcessor::remapExternalMidi (MidiBuffer& midi)
{
    if (midi.isEmpty())
        return;

    MidiBuffer out;

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();

        if (! (m.isNoteOn() || m.isNoteOff()))
        {
            out.addEvent (m, meta.samplePosition);
            continue;
        }

        const int n = m.getNoteNumber();

        if (m.isNoteOn())
        {
            lastMidiNote.store (n);
            const int lp = learnPad.load();
            if (lp >= 0)
            {
                noteMap[(size_t) n].store (lp);
                learnedPad[(size_t) n].store (lp);
                learnPad.store (-1);
            }
        }

        const int pad = noteMap[(size_t) n].load();
        if (pad < 0)
            continue;

        out.addEvent (m.isNoteOn() ? MidiMessage::noteOn (m.getChannel(), df::firstNote + pad, m.getVelocity())
                                   : MidiMessage::noteOff (m.getChannel(), df::firstNote + pad, m.getVelocity()),
                      meta.samplePosition);
    }

    midi.swapWith (out);
}

void DrumForgeProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();

    if (buffer.getNumChannels() < 2)
        return;

    updateBlockState (buffer);
    remapExternalMidi (midi);
    generateGrooveMidi (midi, buffer.getNumSamples());

    // Triggers coming from the GUI
    for (int i = 0; i < df::numPads; ++i)
    {
        const int v = pendingTrigger[(size_t) i].exchange (0);
        if (v > 0)
            startVoice (i, (float) v / 127.0f);
    }

    const int total = buffer.getNumSamples();
    int pos = 0;

    for (const auto meta : midi)
    {
        const int t = jlimit (pos, total, meta.samplePosition);
        render (pos, t - pos);
        pos = t;
        handleMidi (meta.getMessage());
    }

    render (pos, total - pos);

    applyMainFx (total);

    // Demo mode (unlicensed builds only): the output goes silent for 3 s every 40 s
    if (licenseRequired() && ! licensed.load())
    {
        const double period = 40.0 * currentSampleRate, gap = 3.0 * currentSampleRate;
        for (int i = 0; i < total; ++i)
        {
            if (demoSamples >= period - gap)
                for (size_t b = 0; b < outL.size(); ++b) { if (outL[b] != nullptr) outL[b][i] = 0.0f; if (outR[b] != nullptr) outR[b][i] = 0.0f; }
            demoSamples += 1.0;
            if (demoSamples >= period)
                demoSamples = 0.0;
        }
    }

    // Safety soft-clip on the Main bus only (stacked hits can exceed 0 dBFS); the separate outputs stay untouched
    auto softClip = [] (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.8f)
            return x;
        const float y = 0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f);
        return x < 0.0f ? -y : y;
    };

    for (int i = 0; i < total; ++i)
    {
        outL[0][i] = softClip (outL[0][i]);
        outR[0][i] = softClip (outR[0][i]);
    }

    // Meters
    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < total; ++i)
    {
        pl = jmax (pl, std::abs (outL[0][i]));
        pr = jmax (pr, std::abs (outR[0][i]));
    }
    masterPeakL.store (pl);
    masterPeakR.store (pr);

    for (int i = 0; i < df::numPads; ++i)
        padLevel[(size_t) i].store (blockPeak[(size_t) i]);
}

//==============================================================================
void DrumForgeProcessor::handleMidi (const MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int pad = m.getNoteNumber() - df::firstNote;
        if (isPositiveAndBelow (pad, df::numPads))
            startVoice (pad, m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        const int pad = m.getNoteNumber() - df::firstNote;
        if (isPositiveAndBelow (pad, df::numPads) && gateP[(size_t) pad]->load() > 0.5f)
            for (auto& v : voices)
                if (v.active && v.pad == pad && ! v.releasing)
                    releaseVoice (v, 0.02);
    }
    else if (m.isController() && m.getControllerNumber() == 4)
    {
        // hi-hat pedal: closing the pedal cuts the open / half-open hat
        const int prev = hatCC.exchange (m.getControllerValue());
        if (m.getControllerValue() >= 96 && prev < 96)
            for (auto& v : voices)
                if (v.active && v.pad == 11 && ! v.releasing)
                    releaseVoice (v, 0.012);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices)
            if (v.active && ! v.releasing)
                releaseVoice (v, 0.01);
    }
}

void DrumForgeProcessor::releaseVoice (Voice& v, double seconds)
{
    v.releasing = true;
    v.relStep = (float) (1.0 / jmax (1.0, seconds * currentSampleRate));
}

void DrumForgeProcessor::startVoice (int pad, float velocity)
{
    // Hi-hat pedal (CC4): closed / half-open / open chosen by the foot position once a CC4 has been received
    const int cc = hatCC.load();
    bool halfOpen = false;
    if (cc >= 0 && (pad == 9 || pad == 11))
    {
        pad = cc >= 96 ? 9 : 11;
        halfOpen = cc >= 40 && cc < 96;
    }

    std::shared_ptr<const Sample> s;
    {
        const SpinLock::ScopedTryLockType l (slots[(size_t) pad].lock);
        if (! l.isLocked())
            return;
        s = slots[(size_t) pad].sample;
    }

    if (s != nullptr && halfOpen && s->half != nullptr)
        s = s->half;

    if (s == nullptr)
        return;

    // Cymbals: up to four microphone sets mixed together (Dry / Studio / Room / Hall)
    if (s->mics[0] != nullptr || s->mics[1] != nullptr || s->mics[2] != nullptr || s->mics[3] != nullptr)
    {
        // choke the previous hit of this pad / choke group once, before any new voice starts
        chokeFor (pad);
        for (int m = 0; m < 4; ++m)
        {
            const float lvl = micLevel (m);
            if (lvl < 0.002f || s->mics[(size_t) m] == nullptr)
                continue;

            auto set = s->mics[(size_t) m];
            if (halfOpen && set->half != nullptr)
                set = set->half;
            spawnVoice (pad, set, velocity, lvl * lvl * 1.4f, false);
        }
        return;
    }

    chokeFor (pad);
    spawnVoice (pad, s, velocity, 1.0f, true);
}

void DrumForgeProcessor::chokeFor (int pad)
{
    // Choke: same pad retriggers cut the previous hit, plus any pad in the same choke group
    const int choke = (int) std::lround (chokeP[(size_t) pad]->load());
    for (auto& v : voices)
    {
        if (! v.active || v.releasing)
            continue;

        const bool samePad = (v.pad == pad);
        const bool sameGroup = choke > 0
                            && (int) std::lround (chokeP[(size_t) v.pad]->load()) == choke;

        if (samePad || sameGroup)
            releaseVoice (v, 0.005);
    }
}

void DrumForgeProcessor::spawnVoice (int pad, std::shared_ptr<const Sample> s, float velocity, float extra, bool tone)
{
    // Velocity layers (soft / medium / hard)
    if (s != nullptr && ! s->layers.empty())
    {
        std::shared_ptr<const Sample> picked = s->layers.back();
        for (size_t i = 0; i < s->layers.size(); ++i)
            if (velocity <= s->layerMax[i]) { picked = s->layers[i]; break; }
        s = std::move (picked);
    }

    if (s == nullptr || s->data.getNumSamples() == 0)
        return;

    // Find a free voice, otherwise steal the oldest
    Voice* target = nullptr;
    for (auto& v : voices)
        if (! v.active) { target = &v; break; }

    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices)
            if (v.order < target->order)
                target = &v;
    }

    // Humanize: small random velocity and pitch variation per hit
    const float hum = humanizeP->load();
    double semis = (double) pitchP[(size_t) pad]->load();
    float vel = velocity;
    if (hum > 0.0f)
    {
        vel *= 1.0f - hum * 0.3f * rng.nextFloat();
        semis += (double) (hum * 0.25f * (rng.nextFloat() * 2.0f - 1.0f));
    }

    const float decaySec = decayP[(size_t) pad]->load();
    target->decayMul = decaySec < 4.99f ? (float) std::exp (-1.0 / (decaySec * currentSampleRate)) : 1.0f;

    target->sample    = std::move (s);
    target->pad       = pad;
    target->pos       = 0.0;
    target->inc       = target->sample->sampleRate / currentSampleRate * std::pow (2.0, semis / 12.0);
    // Sensitivity: expressive velocity curve (soft hits much quieter) + darker tone on soft hits
    const float sens = sensP != nullptr ? sensP->load() : 0.6f;
    const float curved = std::pow (jlimit (0.0f, 1.0f, vel), 1.0f + sens * 0.9f);
    target->vel       = curved * extra;
    target->toneOn    = tone && sens > 0.01f;
    {
        // cutoff 1.8 kHz (ppp) .. open (fff); one-pole coefficient
        const double fc = 1800.0 + 20000.0 * std::pow ((double) jlimit (0.0f, 1.0f, vel), 1.6);
        target->tone = (float) jlimit (0.05, 1.0, 1.0 - std::exp (-2.0 * MathConstants<double>::pi * fc / currentSampleRate));
        if (sens < 0.5f) target->tone = (float) jmin (1.0, (double) target->tone + (0.5 - sens) * 2.0);
        target->lpL = target->lpR = 0.0f;
    }
    target->env       = 1.0f;
    target->relStep   = 0.0f;
    target->releasing = false;
    target->active    = true;
    target->order     = ++voiceCounter;
}

void DrumForgeProcessor::render (int start, int num)
{
    if (num <= 0)
        return;

    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        const auto& pr = padRT[(size_t) v.pad];
        float* oL = outL[(size_t) pr.bus];
        float* oR = outR[(size_t) pr.bus];

        const auto& s = *v.sample;
        const int len = s.data.getNumSamples();
        const float* dL = s.data.getReadPointer (0);
        const float* dR = s.data.getNumChannels() > 1 ? s.data.getReadPointer (1) : dL;

        float peak = blockPeak[(size_t) v.pad];
        bool finished = false;

        for (int i = 0; i < num; ++i)
        {
            const int i0 = (int) v.pos;
            if (i0 >= len)
            {
                finished = true;
                break;
            }

            const int i1 = jmin (i0 + 1, len - 1);
            const float frac = (float) (v.pos - (double) i0);

            const float l = dL[i0] + (dL[i1] - dL[i0]) * frac;
            const float r = dR[i0] + (dR[i1] - dR[i0]) * frac;
            const float e = v.env * v.vel;
            float l2 = l, r2 = r;
            if (v.toneOn)
            {
                v.lpL += v.tone * (l - v.lpL);
                v.lpR += v.tone * (r - v.lpR);
                l2 = v.lpL; r2 = v.lpR;
            }

            const float sl = l2 * e * pr.gainL;
            const float sr = r2 * e * pr.gainR;

            oL[start + i] += sl;
            oR[start + i] += sr;

            peak = jmax (peak, std::abs (sl), std::abs (sr));

            v.pos += v.inc;

            if (v.decayMul < 1.0f)
            {
                v.env *= v.decayMul;
                if (v.env < 0.0005f)
                {
                    finished = true;
                    break;
                }
            }

            if (v.releasing)
            {
                v.env -= v.relStep;
                if (v.env <= 0.0f)
                {
                    finished = true;
                    break;
                }
            }
        }

        blockPeak[(size_t) v.pad] = peak;

        if (finished)
        {
            v.active = false;
            v.sample.reset();   // keepAlive still owns the sample, so nothing is freed here
        }
    }
}

//==============================================================================
void DrumForgeProcessor::triggerPad (int pad, float velocity)
{
    if (isPositiveAndBelow (pad, df::numPads))
        pendingTrigger[(size_t) pad].store (jlimit (1, 127, (int) (velocity * 127.0f)));
}

// Only samples that live inside the kits folder can be read (unless the dev flag is set)
static bool isAllowedSource (const File& f)
{
    return df::externalSamplesEnabled || f.isAChildOf (DrumForgeProcessor::getKitsFolder());
}

bool DrumForgeProcessor::isSupportedFile (const File& f) const
{
    return isAllowedSource (f) && f.existsAsFile() && formatManager.findFormatForFileExtension (f.getFileExtension()) != nullptr;
}

std::shared_ptr<const Sample> DrumForgeProcessor::getSampleShared (int pad) const
{
    if (! isPositiveAndBelow (pad, df::numPads))
        return nullptr;

    const SpinLock::ScopedLockType l (slots[(size_t) pad].lock);
    return slots[(size_t) pad].sample;
}

String DrumForgeProcessor::getSampleName (int pad) const
{
    if (auto s = getSampleShared (pad))
        return s->name;

    return {};
}

void DrumForgeProcessor::pruneKeepAlive()
{
    // Message thread (under loadLock). Samples nobody else references are freed here,
    // never on the audio thread.
    keepAlive.erase (std::remove_if (keepAlive.begin(), keepAlive.end(),
                                     [] (const std::shared_ptr<const Sample>& p) { return p.use_count() <= 1; }),
                     keepAlive.end());
}

bool DrumForgeProcessor::loadSample (int pad, const File& f)
{
    if (! loadSampleInternal (pad, f))
        return false;

    setCurrentKitName ("Custom");
    return true;
}

bool DrumForgeProcessor::loadSampleInternal (int pad, const File& f)
{
    if (! isAllowedSource (f) || ! isPositiveAndBelow (pad, df::numPads) || ! f.existsAsFile())
        return false;

    std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (f));
    if (reader == nullptr)
        return false;

    lastDirectory = f.getParentDirectory();
    return installSample (pad, std::move (reader), f.getFileNameWithoutExtension(), f, {});
}

bool DrumForgeProcessor::installSample (int pad, std::unique_ptr<AudioFormatReader> reader,
                                        const String& name, const File& file, const String& factoryId)
{
    if (! isPositiveAndBelow (pad, df::numPads) || reader == nullptr || reader->sampleRate <= 0.0)
        return false;

    const auto maxLen = (int64) (reader->sampleRate * 30.0);   // cap at 30 s per pad
    const int len = (int) jmin (reader->lengthInSamples, maxLen);
    if (len <= 0)
        return false;

    const int ch = jmin ((int) reader->numChannels, 2);

    auto s = std::make_shared<Sample>();
    s->data.setSize (ch, len);
    if (! reader->read (&s->data, 0, len, 0, true, ch > 1))
        return false;

    s->sampleRate = reader->sampleRate;
    s->name = name;
    s->file = file;
    s->factoryId = factoryId;

    std::shared_ptr<const Sample> cs = s;
    s.reset();

    {
        const ScopedLock sl (loadLock);
        keepAlive.push_back (cs);

        {
            const SpinLock::ScopedLockType l (slots[(size_t) pad].lock);
            slots[(size_t) pad].sample = cs;
        }

        pruneKeepAlive();
    }

    return true;
}

std::shared_ptr<Sample> DrumForgeProcessor::decodeFactory (const String& original, const String& name)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const char* rn = BinaryData::namedResourceList[i];
        if (String (BinaryData::getNamedResourceOriginalFilename (rn)) != original)
            continue;

        int size = 0;
        const char* data = BinaryData::getNamedResource (rn, size);
        if (data == nullptr || size <= 0)
            return nullptr;

        std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (std::make_unique<MemoryInputStream> (data, (size_t) size, false)));
        if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
            return nullptr;

        const int len = (int) jmin (reader->lengthInSamples, (int64) (reader->sampleRate * 30.0));
        const int ch = jmin ((int) reader->numChannels, 2);

        auto s = std::make_shared<Sample>();
        s->data.setSize (ch, len);
        if (! reader->read (&s->data, 0, len, 0, true, ch > 1))
            return nullptr;

        s->sampleRate = reader->sampleRate;
        s->name = name;
        s->factoryId = original;
        return s;
    }

    return nullptr;
}

void DrumForgeProcessor::publishSample (int pad, std::shared_ptr<const Sample> cs)
{
    const ScopedLock sl (loadLock);

    // the layer samples are kept alive too, so the audio thread never frees audio data
    keepAlive.push_back (cs);
    for (const auto& l : cs->layers)
        keepAlive.push_back (l);
    if (cs->half != nullptr)
        keepAlive.push_back (cs->half);

    {
        const SpinLock::ScopedLockType l (slots[(size_t) pad].lock);
        slots[(size_t) pad].sample = cs;
    }

    pruneKeepAlive();
}

// Builds a cymbal pad from the embedded multi-layer set of the selected microphone
// (files "kx_<mic>_<NN>_<tier>_<Name>.flac", mic d/s/r/h, tier s/m/h, z = half-open hi-hat)
bool DrumForgeProcessor::loadCymbalSet (int pad)
{
    if (pad < firstSharedPad || pad >= df::numPads)
        return false;

    auto parent = std::make_shared<Sample>();
    parent->sampleRate = 48000.0;
    bool any = false;

    for (int mic = 0; mic < 4; ++mic)
    {
        const String prefix = String ("kx_") + "dsrh"[mic] + "_" + String (pad + 1).paddedLeft ('0', 2) + "_";

        struct Found { char tier; String original; };
        std::vector<Found> found;
        String halfFile;

        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const String original = BinaryData::getNamedResourceOriginalFilename (BinaryData::namedResourceList[i]);
            if (original.startsWith (prefix))
            {
                const char t = original[prefix.length()];
                if (t == 'z') halfFile = original; else found.push_back ({ t, original });
            }
        }

        if (found.empty())
            continue;

        std::sort (found.begin(), found.end(), [] (const Found& a, const Found& b)
        {
            auto rank = [] (char c) { return c == 's' ? 0 : c == 'm' ? 1 : 2; };
            return rank (a.tier) < rank (b.tier);
        });

        const String readable = found.front().original.fromFirstOccurrenceOf (prefix, false, false).substring (2)
                                    .upToLastOccurrenceOf (".", false, false).replaceCharacter ('_', ' ');

        auto set = std::make_shared<Sample>();
        set->name = readable;
        set->sampleRate = 48000.0;

        const int n = (int) found.size();
        for (int i = 0; i < n; ++i)
        {
            auto layer = decodeFactory (found[(size_t) i].original, readable);
            if (layer == nullptr)
                return false;

            set->layers.push_back (layer);
            set->layerMax.push_back (n == 1 ? 1.0f : n == 2 ? (i == 0 ? 0.65f : 1.0f)
                                                             : (i == 0 ? 0.40f : i == 1 ? 0.75f : 1.0f));
        }

        if (halfFile.isNotEmpty())
            set->half = decodeFactory (halfFile, "HH Half Open");

        if (mic == 1 || parent->name.isEmpty())
        {
            parent->name = readable;
            for (int i = 0; i < n; ++i)
                if (found[(size_t) i].tier == 'm' || (i == n - 1 && parent->factoryId.isEmpty()))
                    parent->factoryId = "kx_s_" + found[(size_t) i].original.fromFirstOccurrenceOf ("_", false, false)
                                                                              .fromFirstOccurrenceOf ("_", false, false);
        }

        parent->mics[(size_t) mic] = set;
        any = true;
    }

    if (! any)
        return false;

    publishSample (pad, parent);
    return true;
}

bool DrumForgeProcessor::activateLicense (const String& name, const String& email, const String& key)
{
    if (! df::License::verify (name, email, key))
        return false;

    df::License::save (name, email, key);
    {
        const ScopedLock sl (kitLock);
        licenseName = name.trim();
    }
    licensed.store (true);
    return true;
}

void DrumForgeProcessor::reloadCymbals()
{
    for (int pad = firstSharedPad; pad < df::numPads; ++pad)
        if (auto s = getSampleShared (pad))
            if (s->factoryId.startsWith ("kx_"))
                loadCymbalSet (pad);
}

void DrumForgeProcessor::clearSample (int pad)
{
    if (! isPositiveAndBelow (pad, df::numPads))
        return;

    const ScopedLock sl (loadLock);

    {
        const SpinLock::ScopedLockType l (slots[(size_t) pad].lock);
        slots[(size_t) pad].sample.reset();
    }

    pruneKeepAlive();
}

void DrumForgeProcessor::clearAllSamples()
{
    for (int i = 0; i < df::numPads; ++i)
        clearSample (i);

    setCurrentKitName ("Empty");
}

Array<File> DrumForgeProcessor::audioFilesIn (const File& folder) const
{
    Array<File> audio;

    for (const auto& f : folder.findChildFiles (File::findFiles, false))
        if (! f.isHidden() && ! f.getFileName().startsWithChar ('.') && isSupportedFile (f))
            audio.add (f);

    std::sort (audio.begin(), audio.end(), [] (const File& a, const File& b)
    {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });

    return audio;
}

void DrumForgeProcessor::loadFolder (const File& folder)
{
    if (! isAllowedSource (folder) || ! folder.isDirectory())
        return;

    std::array<File, df::numPads> map;
    assignFilesToPads (audioFilesIn (folder), map);

    for (int i = 0; i < df::numPads; ++i)
    {
        if (map[(size_t) i] != File() && loadSampleInternal (i, map[(size_t) i]))
            continue;

        clearSample (i);
    }

    lastDirectory = folder;
    setCurrentKitName (folder.getFileName());
}

//==============================================================================
// Kits
const StringArray& DrumForgeProcessor::getPadNames()
{
    static const StringArray names { "Kick", "Kick 2", "Snare", "Snare Rim", "Side Stick", "Floor Tom",
                                     "Low Tom", "Mid Tom", "High Tom", "HH Closed", "HH Pedal", "HH Open",
                                     "Crash 1", "Splash", "Ride", "Ride Bell",
                                     "Crash 2", "Crash 3", "Splash 2", "China", "FX Cymbal" };
    return names;
}

File DrumForgeProcessor::getKitsFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory)
               .getChildFile ("African Gospel").getChildFile ("African Drum").getChildFile ("Kits");
}

void DrumForgeProcessor::rescanKits()
{
    std::vector<KitInfo> list;

    for (int i = 0; i < numFactoryKits; ++i)
        list.push_back ({ factoryKitNames[i], File(), i });

    const auto folder = getKitsFolder();
    folder.createDirectory();

    auto dirs = folder.findChildFiles (File::findDirectories, false);
    std::sort (dirs.begin(), dirs.end(), [] (const File& a, const File& b)
    {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });

    for (const auto& d : dirs)
        if (! d.isHidden() && ! d.getFileName().startsWithChar ('.') && audioFilesIn (d).size() > 0)
            list.push_back ({ d.getFileName(), d, -1 });

    const ScopedLock sl (kitLock);
    kits = std::move (list);
}

std::vector<KitInfo> DrumForgeProcessor::getKits() const
{
    const ScopedLock sl (kitLock);
    return kits;
}

String DrumForgeProcessor::getCurrentKitName() const
{
    const ScopedLock sl (kitLock);
    return currentKitName;
}

void DrumForgeProcessor::setCurrentKitName (const String& n)
{
    const ScopedLock sl (kitLock);
    currentKitName = n;
}

bool DrumForgeProcessor::loadFactoryResource (int pad, const String& fileNamePrefix)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const char* resName = BinaryData::namedResourceList[i];
        const String original = BinaryData::getNamedResourceOriginalFilename (resName);

        if (! original.startsWith (fileNamePrefix))
            continue;

        int size = 0;
        const char* data = BinaryData::getNamedResource (resName, size);
        if (data == nullptr || size <= 0)
            return false;

        auto stream = std::make_unique<MemoryInputStream> (data, (size_t) size, false);
        std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (std::move (stream)));

        // imported kits show the real sample name, the built-in synthesized ones the pad name
        int kitNo = 0, number = 0; bool spare = false; String readable;
        const bool named = parseFactoryName (original, kitNo, spare, number, readable);

        return installSample (pad, std::move (reader), named ? readable : getPadNames()[pad], File(), original);
    }

    return false;
}

std::vector<FactorySampleInfo> DrumForgeProcessor::getFactorySamples()
{
    std::vector<FactorySampleInfo> out;

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const String original = BinaryData::getNamedResourceOriginalFilename (BinaryData::namedResourceList[i]);
        int kit = 0, number = 0; bool spare = false; String readable;

        if (! (original.endsWith (".wav") || original.endsWith (".flac")) || ! parseFactoryName (original, kit, spare, number, readable))
            continue;

        if (kit > numFactoryKits)
            continue;

        FactorySampleInfo f;
        f.kitIndex = kit == 0 ? numFactoryKits : kit - 1;
        f.kit = kit == 0 ? String ("Shared Cymbals") : String (factoryKitNames[kit - 1]);
        f.name = readable;
        f.pad = number - 1;
        f.resource = original;
        out.push_back (f);
    }

    // imported kits first, then by name
    std::sort (out.begin(), out.end(), [] (const FactorySampleInfo& a, const FactorySampleInfo& b)
    {
        return a.kitIndex != b.kitIndex ? a.kitIndex < b.kitIndex : a.resource.compareNatural (b.resource) < 0;
    });

    return out;
}

bool DrumForgeProcessor::loadKit (int index)
{
    KitInfo kit;
    {
        const ScopedLock sl (kitLock);
        if (! isPositiveAndBelow (index, (int) kits.size()))
            return false;

        kit = kits[(size_t) index];
    }

    if (kit.factoryIndex >= 0)
    {
        for (int pad = 0; pad < df::numPads; ++pad)
        {
            if (pad >= firstSharedPad)
            {
                if (! loadCymbalSet (pad))
                    clearSample (pad);
                continue;
            }

            const String prefix = "k" + String (kit.factoryIndex + 1) + "_" + String (pad + 1).paddedLeft ('0', 2) + "_";
            if (! loadFactoryResource (pad, prefix))
                clearSample (pad);
        }
    }
    else
    {
        std::array<File, df::numPads> map;
        assignFilesToPads (audioFilesIn (kit.folder), map);

        for (int pad = 0; pad < df::numPads; ++pad)
        {
            if (map[(size_t) pad] != File() && loadSampleInternal (pad, map[(size_t) pad]))
                continue;

            clearSample (pad);
        }
    }

    setCurrentKitName (kit.name);
    return true;
}

//==============================================================================
// Mixer presets (per kit)
File DrumForgeProcessor::getPresetsFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory)
               .getChildFile ("African Gospel").getChildFile ("African Drum").getChildFile ("Presets");
}

static const char* const kMixGlobals[] = { "micDry", "micStudio", "micRoom", "micHall", "sens", "roomSize", "roomMix", "humanize" };
static const char* const kMixParams[] = { "gain", "pan", "mute", "solo", "out", "choke", "pitch", "decay", "gate" };

// Five factory mixer presets per factory kit, written once (marker file) into the kit's presets folder
void DrumForgeProcessor::installFactoryMixPresets() const
{
    const auto kit = getCurrentKitName();
    int kitIdx = -1;
    for (int k = 0; k < numFactoryKits; ++k)
        if (kit == factoryKitNames[k]) kitIdx = k;
    if (kitIdx < 0)
        return;

    const auto dir = getPresetsFolder().getChildFile (File::createLegalFileName (kit));
    const auto marker = dir.getChildFile (".factory_v2");
    if (marker.existsAsFile() || ! dir.createDirectory().wasOk())
        return;

    struct Preset
    {
        const char* name;
        float kick, snare, tom, hat, cym;      // dB per group
        float dry, studio, room, hall;         // microphone levels
        float sens, roomSize, roomMix, humanize, cymDecay;   // cymDecay 5 = natural
    };

    // kit-specific character: kick, snare, toms, hats, cymbals (dB)
    const float kitTrim[3][5] = { { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },       // Standard Kit
                                  { 0.0f, -0.5f, 1.0f, 1.5f, 0.5f },      // Soukous Fusion: busier hats / toms
                                  { 1.0f, 1.5f, 0.0f, -0.5f, -0.5f } };   // Seben Kit: punchy kick + snare
    const float kitSens[3] = { 0.60f, 0.70f, 0.65f };

    const Preset presets[] = {
        { "1 Natural",        0.0f,  0.0f,  0.0f,  0.0f, -1.0f,  0.00f, 0.85f, 0.25f, 0.00f,  0.0f, 0.45f, 0.20f, 0.10f, 5.0f },
        { "2 Tight and Dry",  1.0f,  0.5f, -0.5f,  1.0f, -3.0f,  0.80f, 0.40f, 0.00f, 0.00f,  0.1f, 0.20f, 0.08f, 0.05f, 1.6f },
        { "3 Live Room",      0.0f,  0.5f,  1.0f, -1.0f, -1.5f,  0.00f, 0.55f, 0.75f, 0.10f,  0.0f, 0.60f, 0.35f, 0.25f, 5.0f },
        { "4 Gospel Worship", 0.5f,  1.0f,  0.0f, -2.0f, -3.0f,  0.00f, 0.60f, 0.35f, 0.50f, -0.05f, 0.70f, 0.30f, 0.15f, 5.0f },
        { "5 Big Hall",      -0.5f,  0.0f,  0.5f, -2.5f, -2.0f,  0.00f, 0.40f, 0.55f, 0.85f, -0.1f, 0.88f, 0.48f, 0.20f, 5.0f }
    };

    auto norm = [this] (const String& id, float v)
    {
        auto* p = apvts.getParameter (id);
        return p != nullptr ? p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (v)) : 0.0f;
    };

    for (const auto& pr : presets)
    {
        XmlElement root ("MIXPRESET");
        root.setAttribute ("version", 1);

        const float sens = jlimit (0.0f, 1.0f, kitSens[kitIdx] + pr.sens);
        const std::pair<const char*, float> globals[] = {
            { "micDry", pr.dry }, { "micStudio", pr.studio }, { "micRoom", pr.room }, { "micHall", pr.hall },
            { "sens", sens }, { "roomSize", pr.roomSize }, { "roomMix", pr.roomMix }, { "humanize", pr.humanize } };
        for (auto& g : globals)
            root.setAttribute (g.first, (double) norm (g.first, g.second));

        for (int pad = 0; pad < df::numPads; ++pad)
        {
            const int grp = pad <= 1 ? 0 : pad <= 4 ? 1 : pad <= 8 ? 2 : pad <= 11 ? 3 : 4;
            const float groupDb[5] = { pr.kick, pr.snare, pr.tom, pr.hat, pr.cym };
            float db = groupDb[grp] + kitTrim[kitIdx][grp];
            if (pad == 1 || pad == 3 || pad == 4 || pad == 13 || pad == 18)   // secondary pieces a bit lower
                db -= 2.0f;

            auto* e = root.createNewChildElement ("PAD");
            e->setAttribute ("i", pad);
            e->setAttribute ("gain", (double) norm (df::pid ("gain", pad), jlimit (-60.0f, 6.0f, db)));
            e->setAttribute ("mute", 0.0);
            e->setAttribute ("solo", 0.0);
            e->setAttribute ("decay", (double) norm (df::pid ("decay", pad), grp == 4 ? pr.cymDecay : 5.0f));
        }

        root.writeTo (dir.getChildFile (String (pr.name) + ".xml"));
    }

    marker.replaceWithText ("v2");
}

StringArray DrumForgeProcessor::listMixPresets() const
{
    StringArray names;
    installFactoryMixPresets();
    const auto dir = getPresetsFolder().getChildFile (File::createLegalFileName (getCurrentKitName().isEmpty() ? String ("Kit") : getCurrentKitName()));

    for (const auto& f : dir.findChildFiles (File::findFiles, false, "*.xml"))
        names.add (f.getFileNameWithoutExtension());

    names.sort (true);
    return names;
}

bool DrumForgeProcessor::saveMixPreset (const String& name) const
{
    const auto clean = File::createLegalFileName (name.trim());
    if (clean.isEmpty())
        return false;

    const auto dir = getPresetsFolder().getChildFile (File::createLegalFileName (getCurrentKitName().isEmpty() ? String ("Kit") : getCurrentKitName()));
    if (! dir.createDirectory().wasOk())
        return false;

    XmlElement root ("MIXPRESET");
    root.setAttribute ("version", 1);
    for (auto* id : kMixGlobals)
        root.setAttribute (id, apvts.getParameter (id)->getValue());

    for (int pad = 0; pad < df::numPads; ++pad)
    {
        auto* e = root.createNewChildElement ("PAD");
        e->setAttribute ("i", pad);
        for (auto* base : kMixParams)
            if (auto* prm = apvts.getParameter (df::pid (base, pad)))
                e->setAttribute (base, prm->getValue());
    }

    return root.writeTo (dir.getChildFile (clean + ".xml"));
}

bool DrumForgeProcessor::loadMixPreset (const String& name)
{
    installFactoryMixPresets();
    const auto dir = getPresetsFolder().getChildFile (File::createLegalFileName (getCurrentKitName().isEmpty() ? String ("Kit") : getCurrentKitName()));
    std::unique_ptr<XmlElement> root (XmlDocument::parse (dir.getChildFile (File::createLegalFileName (name) + ".xml")));
    if (root == nullptr || ! root->hasTagName ("MIXPRESET"))
        return false;

    for (auto* id : kMixGlobals)
        if (root->hasAttribute (id))
            if (auto* m = apvts.getParameter (id))
                m->setValueNotifyingHost ((float) root->getDoubleAttribute (id));

    for (auto* e : root->getChildWithTagNameIterator ("PAD"))
    {
        const int pad = e->getIntAttribute ("i", -1);
        if (! isPositiveAndBelow (pad, df::numPads))
            continue;

        for (auto* base : kMixParams)
            if (e->hasAttribute (base))
                if (auto* prm = apvts.getParameter (df::pid (base, pad)))
                    prm->setValueNotifyingHost ((float) e->getDoubleAttribute (base));
    }

    return true;
}

bool DrumForgeProcessor::deleteMixPreset (const String& name) const
{
    const auto dir = getPresetsFolder().getChildFile (File::createLegalFileName (getCurrentKitName().isEmpty() ? String ("Kit") : getCurrentKitName()));
    return dir.getChildFile (File::createLegalFileName (name) + ".xml").deleteFile();
}

//==============================================================================
void DrumForgeProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    if (xml == nullptr)
        return;

    xml->setAttribute ("midiMap", getMidiMapId());
    {
        String learned;
        for (int n = 0; n < 128; ++n)
            if (learnedPad[(size_t) n].load() >= 0)
                learned << String (n) << ":" << String (learnedPad[(size_t) n].load()) << ",";
        xml->setAttribute ("midiLearn", learned);
    }

    auto* samples = xml->createNewChildElement ("SAMPLES");
    samples->setAttribute ("kit", getCurrentKitName());

    for (int i = 0; i < df::numPads; ++i)
    {
        if (auto s = getSampleShared (i))
        {
            auto* e = samples->createNewChildElement ("SAMPLE");
            e->setAttribute ("pad", i);

            if (s->factoryId.isNotEmpty())
                e->setAttribute ("factory", s->factoryId);
            else
                e->setAttribute ("path", s->file.getFullPathName());
        }
    }

    copyXmlToBinary (*xml, dest);
}

void DrumForgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    std::array<String, df::numPads> paths, factoryIds;
    String kitName = "Custom";

    if (auto* samples = xml->getChildByName ("SAMPLES"))
    {
        kitName = samples->getStringAttribute ("kit", "Custom");

        for (auto* e : samples->getChildWithTagNameIterator ("SAMPLE"))
        {
            const int pad = e->getIntAttribute ("pad", -1);
            if (isPositiveAndBelow (pad, df::numPads))
            {
                paths[(size_t) pad] = e->getStringAttribute ("path");
                factoryIds[(size_t) pad] = e->getStringAttribute ("factory");
            }
        }

        xml->removeChildElement (samples, true);
    }

    {
        const auto mapId = xml->getStringAttribute ("midiMap", "gm");
        const auto learned = xml->getStringAttribute ("midiLearn");
        setMidiMap (mapId);

        for (auto& tok : StringArray::fromTokens (learned, ",", ""))
        {
            const int n = tok.upToFirstOccurrenceOf (":", false, false).getIntValue();
            const int pd = tok.fromFirstOccurrenceOf (":", false, false).getIntValue();
            if (isPositiveAndBelow (n, 128) && isPositiveAndBelow (pd, df::numPads))
            {
                noteMap[(size_t) n].store (pd);
                learnedPad[(size_t) n].store (pd);
            }
        }
    }

    apvts.replaceState (ValueTree::fromXml (*xml));

    for (int i = 0; i < df::numPads; ++i)
    {
        const auto idx = (size_t) i;

        if (factoryIds[idx].startsWith ("kx_") && loadCymbalSet (i))
            continue;

        if (factoryIds[idx].isNotEmpty() && loadFactoryResource (i, factoryIds[idx]))
            continue;

        const File f (paths[idx]);
        if (paths[idx].isNotEmpty() && loadSampleInternal (i, f))
            continue;

        clearSample (i);
    }

    setCurrentKitName (kitName);
}

AudioProcessorEditor* DrumForgeProcessor::createEditor()
{
    return new DrumForgeEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumForgeProcessor();
}


//==============================================================================
// Smart mapping of a folder of samples onto the 16 pads. Real-world kits (DW, Sonor, Mapex,
// Tama...) are usually named after the drum ("Kick_01.wav", "Snare Rim.wav", "Tom Floor.wav").
// When enough names are recognised they are placed on the matching pads, otherwise the files
// are assigned alphabetically to pads 1-16.
void DrumForgeProcessor::assignFilesToPads (const Array<File>& files, std::array<File, df::numPads>& map) const
{
    for (auto& f : map)
        f = File();

    auto has = [] (const String& n, std::initializer_list<const char*> keys)
    {
        for (auto* k : keys)
            if (n.contains (k))
                return true;
        return false;
    };

    Array<File> kicks, snares, toms, hatsClosed, hatsPedal, hatsOpen, hatsOther, crashes, rides, bells, rims, sticks;
    int recognised = 0;

    for (const auto& f : files)
    {
        const auto n = f.getFileNameWithoutExtension().toLowerCase();

        if      (has (n, { "kick", "bassdrum", "bass drum", "_bd", "bd_", " bd" })) kicks.add (f);
        else if (has (n, { "sidestick", "side stick", "side_stick", "xstick", "cross stick" })) sticks.add (f);
        else if (has (n, { "rim" }) && has (n, { "snare", "sn" })) rims.add (f);
        else if (has (n, { "snare", "snr" }))   snares.add (f);
        else if (has (n, { "tom" }))            toms.add (f);
        else if (has (n, { "hat", "hh" }))
        {
            if      (has (n, { "pedal", "foot" }))                    hatsPedal.add (f);
            else if (has (n, { "open", "_op", "-op", " op" }))        hatsOpen.add (f);
            else if (has (n, { "closed", "close", "_cl", "-cl", " cl", "tight" })) hatsClosed.add (f);
            else                                                      hatsOther.add (f);
        }
        else if (has (n, { "bell" }))           bells.add (f);
        else if (has (n, { "ride" }))           rides.add (f);
        else if (has (n, { "crash", "china", "splash" })) crashes.add (f);
        else continue;

        ++recognised;
    }

    if (recognised < 4)
    {
        for (int i = 0; i < df::numPads && i < files.size(); ++i)
            map[(size_t) i] = files[i];
        return;
    }

    auto put = [&map] (int pad, const Array<File>& list, int idx)
    {
        if (idx < list.size() && map[(size_t) pad] == File())
            map[(size_t) pad] = list[idx];
    };

    put (0, kicks, 0);  put (1, kicks, 1);
    put (2, snares, 0); put (3, rims, 0); put (4, sticks, 0);
    if (map[3] == File()) put (3, snares, 1);

    // toms: low to high by name if the names say floor/low/mid/high, otherwise in file order
    Array<File> t = toms;
    auto rank = [] (const File& f)
    {
        const auto n = f.getFileNameWithoutExtension().toLowerCase();
        if (n.contains ("floor")) return 0;
        if (n.contains ("low"))   return 1;
        if (n.contains ("mid"))   return 2;
        if (n.contains ("high") || n.contains ("hi")) return 3;
        return 4;
    };
    std::stable_sort (t.begin(), t.end(), [&rank] (const File& a, const File& b) { return rank (a) < rank (b); });
    for (int i = 0; i < 4; ++i) put (5 + i, t, i);

    put (9,  hatsClosed.isEmpty() ? hatsOther : hatsClosed, 0);
    put (10, hatsPedal, 0);
    put (11, hatsOpen, 0);
    put (12, crashes, 0); put (13, crashes, 1); put (16, crashes, 2); put (17, crashes, 3);
    put (14, rides, 0);   put (15, bells, 0);
}

//==============================================================================
// MIDI groove library
namespace
{
    String prettySection (const String& raw)
    {
        // "01_Intro" -> "Intro"
        const auto name = raw.fromFirstOccurrenceOf ("_", false, false);
        if (name == "FullSong")  return "Full Song";
        if (name == "PreChorus") return "Pre-Chorus";
        if (name == "FillA")     return "Fill A";
        if (name == "FillB")     return "Fill B";
        if (name == "FillC")     return "Fill C";
        return name;
    }

    bool splitGrooveName (const String& resource, String& genre, String& groove, String& section)
    {
        // NB: fromTokens() would treat every "_" as a separator, so split on the double underscore manually
        const auto parts = StringArray::fromTokens (resource.upToLastOccurrenceOf (".", false, false).replace ("__", "\n"), "\n", "");
        if (parts.size() != 3)
            return false;

        genre = parts[0];
        groove = parts[1].replaceCharacter ('_', ' ');
        section = parts[2];
        return true;
    }
}


namespace
{
    // General MIDI drum map -> African Drum pads (C1..D#2); -1 = ignored
    int gmToPad (int n)
    {
        switch (n)
        {
            case 35: case 36: return 36;     // kick
            case 37:          return 40;     // side stick
            case 38: case 40: return 38;     // snare / electric snare
            case 39:          return 39;     // clap -> rim
            case 41: case 43: return 41;     // floor toms
            case 45:          return 42;     // low tom
            case 47:          return 43;     // low-mid tom
            case 48: case 50: return 44;     // high toms
            case 42:          return 45;     // closed hat
            case 44:          return 46;     // pedal hat
            case 46:          return 47;     // open hat
            case 49: return 48;              // crash 1
            case 55: case 57: case 52: return 49;     // splash / crash 2 / china
            case 51: case 59: return 50;     // ride
            case 53: case 56: return 51;     // ride bell
            default:          return -1;
        }
    }

    bool isMidiFile (const File& f) { return f.hasFileExtension ("mid;midi;MID;MIDI"); }

    String userKey (const File& f, bool gm) { return "user:" + f.getFullPathName() + (gm ? "#gm" : "#raw"); }

    bool parseUserKey (const String& key, File& file, bool& gm)
    {
        if (! key.startsWith ("user:"))
            return false;

        String path = key.fromFirstOccurrenceOf ("user:", false, false);
        gm = path.endsWith ("#gm");
        path = path.upToLastOccurrenceOf ("#", false, false);
        file = File (path);
        return true;
    }
}

File DrumForgeProcessor::getUserMidiFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory)
               .getChildFile ("African Gospel").getChildFile ("African Drum").getChildFile ("MIDI Import");
}

std::vector<GenreEntry> DrumForgeProcessor::scanUserMidi (bool gmMap)
{
    std::vector<GenreEntry> out;
    const auto root = getUserMidiFolder();
    if (! root.isDirectory())
        return out;

    GenreEntry genre { "My MIDI", {} };

    // loose files: one groove per file
    for (const auto& f : root.findChildFiles (File::findFiles, false, "*"))
        if (isMidiFile (f))
            genre.grooves.push_back ({ f.getFileNameWithoutExtension(), { { "Full", userKey (f, gmMap) } } });

    // sub-folders: one groove per folder, one section per file
    for (const auto& d : root.findChildFiles (File::findDirectories, false, "*"))
    {
        GrooveEntry g { d.getFileName(), {} };
        auto files = d.findChildFiles (File::findFiles, false, "*");
        std::sort (files.begin(), files.end(), [] (const File& a, const File& b)
                   { return a.getFileName().compareNatural (b.getFileName()) < 0; });

        for (const auto& f : files)
            if (isMidiFile (f))
                g.sections.push_back ({ f.getFileNameWithoutExtension(), userKey (f, gmMap) });

        if (! g.sections.empty())
            genre.grooves.push_back (std::move (g));
    }

    std::sort (genre.grooves.begin(), genre.grooves.end(), [] (const GrooveEntry& a, const GrooveEntry& b)
               { return a.name.compareNatural (b.name) < 0; });

    if (! genre.grooves.empty())
        out.push_back (std::move (genre));

    return out;
}

int DrumForgeProcessor::importMidiFiles (const Array<File>& items)
{
    const auto root = getUserMidiFolder();
    root.createDirectory();
    int n = 0;

    for (const auto& item : items)
    {
        if (item.isDirectory())
        {
            const auto dest = root.getChildFile (item.getFileName());
            dest.createDirectory();

            for (const auto& f : item.findChildFiles (File::findFiles, true, "*"))
                if (isMidiFile (f) && f.copyFileTo (dest.getChildFile (f.getFileName())))
                    ++n;
        }
        else if (isMidiFile (item) && item.copyFileTo (root.getChildFile (item.getFileName())))
            ++n;
    }

    return n;
}

const std::vector<GenreEntry>& DrumForgeProcessor::getGrooveLibrary()
{
    static const std::vector<GenreEntry> lib = []
    {
        std::vector<String> names;
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const String orig = BinaryData::getNamedResourceOriginalFilename (BinaryData::namedResourceList[i]);
            if (orig.endsWith (".mid"))
                names.push_back (orig);
        }

        std::sort (names.begin(), names.end(), [] (const String& a, const String& b) { return a.compareNatural (b) < 0; });

        std::vector<GenreEntry> out;
        for (const auto& n : names)
        {
            String genre, groove, section;
            if (! splitGrooveName (n, genre, groove, section))
                continue;

            genre = genre.replaceCharacter ('_', ' ');

            auto g = std::find_if (out.begin(), out.end(), [&] (const GenreEntry& e) { return e.name == genre; });
            if (g == out.end())
            {
                out.push_back ({ genre, {} });
                g = out.end() - 1;
            }

            auto r = std::find_if (g->grooves.begin(), g->grooves.end(), [&] (const GrooveEntry& e) { return e.name == groove; });
            if (r == g->grooves.end())
            {
                g->grooves.push_back ({ groove, {} });
                r = g->grooves.end() - 1;
            }

            r->sections.push_back ({ genre == "Fills" ? String ("Fill") : prettySection (section), n });
        }

        // "Fills" last
        std::stable_partition (out.begin(), out.end(), [] (const GenreEntry& e) { return e.name != "Fills"; });
        return out;
    }();

    return lib;
}

String DrumForgeProcessor::getGrooveDisplayName (const String& resource)
{
    File uf; bool gm = false;
    if (parseUserKey (resource, uf, gm))
        return "My MIDI - " + uf.getParentDirectory().getFileName() + " / " + uf.getFileNameWithoutExtension();

    String g, r, s;
    if (! splitGrooveName (resource, g, r, s))
        return resource;

    return g.replaceCharacter ('_', ' ') + " - " + r + " - " + (g == "Fills" ? String ("Fill") : prettySection (s));
}

static const char* findEmbedded (const String& original, int& size)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const char* rn = BinaryData::namedResourceList[i];
        if (String (BinaryData::getNamedResourceOriginalFilename (rn)) == original)
            return BinaryData::getNamedResource (rn, size);
    }

    size = 0;
    return nullptr;
}

const GrooveSeq* DrumForgeProcessor::loadGrooveSeq (const String& resource)
{
    for (const auto& k : seqKeep)
        if (k->resource == resource)
            return k.get();

    MidiFile mf;
    File userFile; bool gm = false;
    const bool isUser = parseUserKey (resource, userFile, gm);

    if (isUser)
    {
        FileInputStream in (userFile);
        if (! in.openedOk() || ! mf.readFrom (in))
            return nullptr;
    }
    else
    {
        int size = 0;
        const char* data = findEmbedded (resource, size);
        if (data == nullptr || size <= 0)
            return nullptr;

        MemoryInputStream in (data, (size_t) size, false);
        if (! mf.readFrom (in))
            return nullptr;
    }

    if (mf.getNumTracks() < 1)
        return nullptr;

    const int ppq = mf.getTimeFormat();
    if (ppq <= 0)
        return nullptr;

    auto s = std::make_unique<GrooveSeq>();
    s->resource = resource;

    bool gotTempo = false;
    double lastBeat = 0.0, endBeat = 0.0;

    for (int t = 0; t < mf.getNumTracks(); ++t)
    {
        const auto* track = mf.getTrack (t);

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& m = track->getEventPointer (i)->message;
            const double beat = m.getTimeStamp() / (double) ppq;

            if (m.isNoteOnOrOff())
            {
                MidiMessage msg = m;

                if (isUser && gm)
                {
                    const int pad = gmToPad (m.getNoteNumber());
                    if (pad < 0)
                        continue;

                    msg = m.isNoteOn() ? MidiMessage::noteOn (10, pad, m.getVelocity())
                                       : MidiMessage::noteOff (10, pad);
                }
                else if (isUser && ! isPositiveAndBelow (m.getNoteNumber() - df::firstNote, df::numPads))
                    continue;

                s->events.push_back ({ beat, msg });
                lastBeat = jmax (lastBeat, beat);
            }
            else if (m.isTempoMetaEvent() && ! gotTempo)
            {
                s->bpm = 60.0 / m.getTempoSecondsPerQuarterNote();
                gotTempo = true;
            }
            else if (m.isEndOfTrackMetaEvent())
                endBeat = jmax (endBeat, beat);
        }
    }

    if (s->events.empty())
        return nullptr;

    // length: end-of-track if present, else round up to whole 4-beat bars
    s->lengthBeats = endBeat >= lastBeat && endBeat > 0.0 ? jmax (1.0, endBeat)
                                                           : jmax (4.0, std::ceil (lastBeat / 4.0 + 1e-6) * 4.0);

    std::stable_sort (s->events.begin(), s->events.end(), [] (const GrooveSeq::Ev& a, const GrooveSeq::Ev& b)
    {
        return a.beat < b.beat;
    });

    const GrooveSeq* result = s.get();
    seqKeep.push_back (std::move (s));

    // Old sequences can go, but never the one the audio thread is using nor the 8 most recent ones.
    const GrooveSeq* active = activeSeq.load();
    for (size_t i = 0; seqKeep.size() > 64 && i + 8 < seqKeep.size();)
    {
        if (seqKeep[i].get() == active)
            ++i;
        else
            seqKeep.erase (seqKeep.begin() + (std::ptrdiff_t) i);
    }

    return result;
}

bool DrumForgeProcessor::startGroove (const String& resource, bool restart)
{
    const GrooveSeq* seq = loadGrooveSeq (resource);
    if (seq == nullptr)
        return false;

    grooveBpm.store (seq->bpm);
    if (restart)
        grooveReset.store (true);

    activeSeq.store (seq);
    groovePlaying.store (true);
    return true;
}

void DrumForgeProcessor::stopGroove()
{
    groovePlaying.store (false);
    grooveProgress.store (0.0f);
}

void DrumForgeProcessor::generateGrooveMidi (MidiBuffer& midi, int numSamples)
{
    const GrooveSeq* seq = activeSeq.load();

    if (! groovePlaying.load() || seq == nullptr || numSamples <= 0)
        return;

    if (grooveReset.exchange (false))
        groovePos = 0.0;

    double bpm = seq->bpm;

    if (grooveSyncHost.load())
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto hb = pos->getBpm())
                    if (*hb > 20.0 && *hb < 400.0)
                        bpm = *hb;

    grooveBpm.store (bpm);

    const double blockBeats = (double) numSamples * bpm / (60.0 * currentSampleRate);
    const double len = seq->lengthBeats;
    double start = groovePos;
    double remaining = blockBeats;
    double offsetBeats = 0.0;

    auto emit = [&] (double from, double to, double base)
    {
        for (const auto& e : seq->events)
        {
            if (e.beat < from)
                continue;
            if (e.beat >= to)
                break;

            const int sp = jlimit (0, numSamples - 1, (int) ((base + (e.beat - from)) / blockBeats * numSamples));
            midi.addEvent (e.msg, sp);
        }
    };

    while (remaining > 0.0)
    {
        const double room = len - start;

        if (remaining < room)
        {
            emit (start, start + remaining, offsetBeats);
            start += remaining;
            remaining = 0.0;
        }
        else
        {
            emit (start, len, offsetBeats);
            offsetBeats += room;
            remaining -= room;
            start = 0.0;

            if (! grooveLoop.load())
            {
                groovePlaying.store (false);
                groovePos = 0.0;
                grooveProgress.store (0.0f);
                return;
            }
        }
    }

    groovePos = start;
    grooveProgress.store ((float) (start / len));
}

File DrumForgeProcessor::writeGrooveFile (const String& resource, const File& folder) const
{
    File uf; bool gm = false;
    if (parseUserKey (resource, uf, gm))
    {
        folder.createDirectory();
        const auto dest = folder.getChildFile (uf.getFileName());
        return uf.copyFileTo (dest) ? dest : File();
    }

    int size = 0;
    const char* data = findEmbedded (resource, size);
    if (data == nullptr || size <= 0)
        return {};

    folder.createDirectory();
    const auto f = folder.getChildFile (resource);

    if (! f.replaceWithData (data, (size_t) size))
        return {};

    return f;
}

int DrumForgeProcessor::exportGrooveLibrary (const File& folder) const
{
    int n = 0;

    for (const auto& genre : getGrooveLibrary())
        for (const auto& groove : genre.grooves)
            for (const auto& sec : groove.sections)
                if (writeGrooveFile (sec.resource, folder.getChildFile (genre.name).getChildFile (groove.name)) != File())
                    ++n;

    return n;
}

//==============================================================================
void DrumForgeProcessor::applyMainFx (int total)
{
    float* L = outL[0];
    float* R = outR[0];
    const double sr = currentSampleRate;

    // ---- 3-band EQ
    if (fx.eqOn->load() > 0.5f)
    {
        const float lo = fx.eqLow->load(), mid = fx.eqMid->load(), hi = fx.eqHigh->load(), f = fx.eqFreq->load();

        if (lo != eqCache[0] || mid != eqCache[1] || hi != eqCache[2] || f != eqCache[3])
        {
            eqCache[0] = lo; eqCache[1] = mid; eqCache[2] = hi; eqCache[3] = f;

            for (int c = 0; c < 2; ++c)
            {
                eqLowF[c].setCoefficients (IIRCoefficients::makeLowShelf (sr, 120.0, 0.7, Decibels::decibelsToGain (lo)));
                eqMidF[c].setCoefficients (IIRCoefficients::makePeakFilter (sr, jmin ((double) f, sr * 0.45), 0.9, Decibels::decibelsToGain (mid)));
                eqHighF[c].setCoefficients (IIRCoefficients::makeHighShelf (sr, jmin (7000.0, sr * 0.45), 0.7, Decibels::decibelsToGain (hi)));
            }
        }

        eqLowF[0].processSamples (L, total);  eqLowF[1].processSamples (R, total);
        eqMidF[0].processSamples (L, total);  eqMidF[1].processSamples (R, total);
        eqHighF[0].processSamples (L, total); eqHighF[1].processSamples (R, total);
    }

    // ---- compressor (linked stereo, feed-forward)
    float grMax = 0.0f;
    if (fx.compOn->load() > 0.5f)
    {
        const float thr = fx.compThr->load(), ratio = jmax (1.0f, fx.compRatio->load());
        const float att = std::exp (-1.0f / (float) (jmax (0.05f, fx.compAtt->load()) * 0.001 * sr));
        const float rel = std::exp (-1.0f / (float) (jmax (5.0f, fx.compRel->load()) * 0.001 * sr));
        const float makeup = fx.compMakeup->load();

        for (int i = 0; i < total; ++i)
        {
            const float lvl = Decibels::gainToDecibels (jmax (std::abs (L[i]), std::abs (R[i])), -100.0f);
            const float over = lvl - thr;
            const float target = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
            const float k = target > compEnv ? att : rel;
            compEnv = target + k * (compEnv - target);
            grMax = jmax (grMax, compEnv);

            const float g = Decibels::decibelsToGain (makeup - compEnv);
            L[i] *= g;
            R[i] *= g;
        }
    }
    else
        compEnv = 0.0f;

    compReduction.store (grMax);

    // ---- saturation
    if (fx.satOn->load() > 0.5f)
    {
        const float drive = 1.0f + fx.satDrive->load() * 14.0f;
        const float mix = fx.satMix->load();
        const float norm = 1.0f / std::sqrt (drive);

        for (int i = 0; i < total; ++i)
        {
            L[i] += (std::tanh (L[i] * drive) * norm - L[i]) * mix;
            R[i] += (std::tanh (R[i] * drive) * norm - R[i]) * mix;
        }
    }

    // ---- ping-pong delay with a darker feedback path
    if (fx.dlyOn->load() > 0.5f && ! dlyL.empty())
    {
        const int size = (int) dlyL.size();
        const int d = jlimit (1, size - 2, (int) (fx.dlyTime->load() * 0.001 * sr));
        const float fb = fx.dlyFb->load(), mix = fx.dlyMix->load();

        for (int i = 0; i < total; ++i)
        {
            int rd = dlyPos - d;
            if (rd < 0) rd += size;

            const float dl = dlyL[(size_t) rd], dr = dlyR[(size_t) rd];
            dlyLpL += 0.45f * (dl - dlyLpL);
            dlyLpR += 0.45f * (dr - dlyLpR);

            dlyL[(size_t) dlyPos] = L[i] + dlyLpR * fb;   // cross-feed = ping-pong
            dlyR[(size_t) dlyPos] = R[i] + dlyLpL * fb;

            L[i] += dl * mix;
            R[i] += dr * mix;

            if (++dlyPos >= size) dlyPos = 0;
        }
    }

    // ---- room reverb
    const float roomMix = roomMixP->load();
    if (roomMix > 0.001f)
    {
        Reverb::Parameters rp;
        rp.roomSize = roomSizeP->load();
        rp.damping = fx.roomDamp->load();
        rp.width = 1.0f;
        rp.dryLevel = 1.0f;
        rp.wetLevel = roomMix * 0.5f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        reverb.processStereo (L, R, total);
    }

    // ---- stereo width (mid/side)
    const float w = fx.width->load();
    if (std::abs (w - 1.0f) > 0.002f)
    {
        for (int i = 0; i < total; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]), sd = 0.5f * (L[i] - R[i]) * w;
            L[i] = m + sd;
            R[i] = m - sd;
        }
    }
}
