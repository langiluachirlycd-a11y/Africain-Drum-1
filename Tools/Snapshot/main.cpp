// Renders the real African Drum editor + audio engine offline:
//   Snapshot <outDir> [frames]
// Writes frame_0000.png ... , poster.png and audio.wav (the plugin's actual main output).
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

template <typename T>
static void findAll (Component& c, std::vector<T*>& out)
{
    for (auto* ch : c.getChildren())
    {
        if (auto* t = dynamic_cast<T*> (ch))
            out.push_back (t);
        findAll (*ch, out);
    }
}

static void pump (int ms) { MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static bool selectByText (ComboBox& box, const String& text)
{
    for (int i = 0; i < box.getNumItems(); ++i)
        if (box.getItemText (i) == text)
        {
            box.setSelectedItemIndex (i, sendNotification);
            return true;
        }

    return false;
}

static void selectInAnyCombo (Component& root, const String& text)
{
    std::vector<ComboBox*> combos;
    findAll (root, combos);
    for (auto* c : combos)
        if (selectByText (*c, text))
            return;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;

    const File out = File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "out");
    out.createDirectory();
    const int totalFrames = argc > 2 ? String (argv[2]).getIntValue() : 0;

    constexpr double sr = 44100.0;
    DrumForgeProcessor proc;
    proc.setRateAndBufferSizeDetails (sr, 512);
    proc.prepareToPlay (sr, 512);
    proc.grooveSyncHost.store (false);

    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (1200, 780);
    ed->setVisible (true);

    std::vector<TextButton*> buttons;

    auto clickButton = [&] (const String& text)
    {
        buttons.clear();
        findAll (*ed, buttons);
        for (auto* b : buttons)
            if (b->getButtonText() == text)
            {
                b->triggerClick();
                return true;
            }
        return false;
    };

    selectInAnyCombo (*ed, "Rock");
    pump (150);                       // combo notifications are asynchronous
    selectInAnyCombo (*ed, "Driving 8ths");
    pump (150);

    // select the snare on the drum view, like a click would
    {
        std::vector<DrumKitView*> views;
        findAll (*ed, views);
        if (! views.empty())
        {
            views[0]->setSelectedPad (2);
            if (views[0]->onPadSelected)
                views[0]->onPadSelected (2);
        }
    }

    auto savePng = [&] (const File& f)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        f.deleteFile();
        FileOutputStream fo (f);
        PNGImageFormat().writeImageToStream (img, fo);
    };

    AudioBuffer<float> audio;
    const int frameSamples = (int) (sr / 30.0);
    AudioBuffer<float> block (32, frameSamples);

    std::vector<std::pair<int, int>> pendingNotes;
    std::vector<std::pair<int, int>> pendingCC;

    auto runFrame = [&]
    {
        block.clear();
        MidiBuffer midi;
        for (auto& cc : pendingCC)
            midi.addEvent (MidiMessage::controllerEvent (1, cc.first, cc.second), 0);
        pendingCC.clear();
        for (auto& n : pendingNotes)
            midi.addEvent (MidiMessage::noteOn (1, n.first, (uint8) n.second), 1);
        pendingNotes.clear();
        proc.processBlock (block, midi);
        const int old = audio.getNumSamples();
        audio.setSize (2, old + frameSamples, true, true, true);
        audio.copyFrom (0, old, block, 0, 0, frameSamples);
        audio.copyFrom (1, old, block, 1, 0, frameSamples);
    };

    if (argc > 3)
    {
        // ---- tutorial mode: storyboard file with lines "<frame> <command> [args]" (arguments separated by '|')
        struct Cmd { int frame; StringArray args; };
        std::vector<Cmd> cmds;

        for (auto& line : StringArray::fromLines (File (argv[3]).loadFileAsString()))
        {
            const auto t = line.trim();
            if (t.isEmpty() || t.startsWith ("#"))
                continue;

            StringArray parts;
            parts.addTokens (t, "|", "");
            Cmd c;
            c.frame = parts[0].trim().getIntValue();
            parts.remove (0);
            for (auto& a : parts) a = a.trim();
            c.args = parts;
            cmds.push_back (c);
        }

        std::vector<DrumKitView*> views;
        std::vector<GrooveBrowser*> browsers;
        findAll (*ed, views);
        findAll (*ed, browsers);

        for (int f = 0; f < totalFrames; ++f)
        {
            for (auto& c : cmds)
            {
                if (c.frame != f)
                    continue;

                const auto cmd = c.args[0];

                if (cmd == "page")             clickButton (c.args[1]);
                else if (cmd == "load")        proc.loadSample (c.args[1].getIntValue(), File (c.args[2]));
                else if (cmd == "trigger")     proc.triggerPad (c.args[1].getIntValue(), c.args.size() > 2 ? c.args[2].getIntValue() / 127.0f : 0.9f);
                else if (cmd == "select" && ! views.empty())
                {
                    views[0]->setSelectedPad (c.args[1].getIntValue());
                    if (views[0]->onPadSelected) views[0]->onPadSelected (c.args[1].getIntValue());
                }
                else if (cmd == "makekit")
                {
                    const auto dest = DrumForgeProcessor::getKitsFolder().getChildFile (c.args[2]);
                    dest.deleteRecursively();
                    File (c.args[1]).copyDirectoryTo (dest);
                    proc.rescanKits();
                }
                else if (cmd == "kit")
                {
                    const auto kits = proc.getKits();
                    for (size_t i = 0; i < kits.size(); ++i)
                        if (kits[i].name == c.args[1])
                            proc.loadKit ((int) i);
                }
                else if (cmd == "importmidi" && ! browsers.empty())
                    browsers[0]->importFiles (Array<File> { File (c.args[1]) });
                else if (cmd == "swap")        std::fprintf (stderr, "swap pad %s -> %s\n", c.args[1].toRawUTF8(), proc.loadFactorySample (c.args[1].getIntValue(), c.args[2]) ? "ok" : "FAILED");
                else if (cmd == "name")        std::fprintf (stderr, "pad %s = '%s'  kit '%s'\n", c.args[1].toRawUTF8(), proc.getSampleName (c.args[1].getIntValue()).toRawUTF8(), proc.getCurrentKitName().toRawUTF8());
                else if (cmd == "midimap")     proc.setMidiMap (c.args[1]);
                else if (cmd == "note")        pendingNotes.push_back ({ c.args[1].getIntValue(), c.args.size() > 2 ? c.args[2].getIntValue() : 100 });
                else if (cmd == "licensecheck") { std::cout << "KEY " << df::License::makeKey (c.args[1], c.args[2]) << std::endl; }
                else if (cmd == "cc")          pendingCC.push_back ({ c.args[1].getIntValue(), c.args[2].getIntValue() });
                else if (cmd == "mic")         { const char* ids[] = { "micDry", "micStudio", "micRoom", "micHall" }; for (int k = 0; k < 4; ++k) proc.apvts.getParameter (ids[k])->setValueNotifyingHost (k == c.args[1].getIntValue() ? 0.9f : 0.0f); }
                else if (cmd == "layerinfo")
                {
                    if (auto s = proc.peekSample (c.args[1].getIntValue()))
                    {
                        String o;
                        for (size_t i = 0; i < s->layers.size(); ++i)
                            o << "[" << s->layers[i]->factoryId << " <=" << String (s->layerMax[i], 2) << " " << s->layers[i]->data.getNumSamples() << "smp] ";
                        std::fprintf (stderr, "pad %s '%s' layers: %s half=%d\n", c.args[1].toRawUTF8(), s->name.toRawUTF8(), o.toRawUTF8(), s->half != nullptr ? 1 : 0);
                    }
                    else std::fprintf (stderr, "pad %s empty\n", c.args[1].toRawUTF8());
                }
                else if (cmd == "savemix")     std::fprintf (stderr, "savemix %s -> %d\n", c.args[1].toRawUTF8(), (int) proc.saveMixPreset (c.args[1]));
                else if (cmd == "loadmix")     std::fprintf (stderr, "loadmix %s -> %d  list=%s\n", c.args[1].toRawUTF8(), (int) proc.loadMixPreset (c.args[1]), proc.listMixPresets().joinIntoString (",").toRawUTF8());
                else if (cmd == "tune")        { auto* p = proc.apvts.getParameter (df::pid ("pitch", c.args[1].getIntValue())); p->setValueNotifyingHost (p->convertTo0to1 (c.args[2].getFloatValue())); }
                else if (cmd == "setgain")     proc.apvts.getParameter (df::pid ("gain", c.args[1].getIntValue()))->setValueNotifyingHost (c.args[2].getFloatValue());
                else if (cmd == "gain")        std::fprintf (stderr, "gain pad %s = %.3f\n", c.args[1].toRawUTF8(), proc.apvts.getParameter (df::pid ("gain", c.args[1].getIntValue()))->getValue());
                else if (cmd == "learn")       proc.startMidiLearn (c.args[1].getIntValue());
                else if (cmd == "levels")
                {
                    String o;
                    for (int i = 0; i < df::numPads; ++i)
                        if (proc.padLevel[(size_t) i].load() > 0.001f) o << (i + 1) << " ";
                    std::fprintf (stderr, "frame %d map=%s pads sounding: %s\n", f, proc.getMidiMapId().toRawUTF8(), o.toRawUTF8());
                }
                else if (cmd == "stopgroove")  proc.stopGroove();
                else if (cmd == "startgroove") clickButton ("PLAY");
            }

            runFrame();
            pump (36);
            savePng (out.getChildFile ("frame_" + String (f).paddedLeft ('0', 4) + ".png"));
        }

        WavAudioFormat wavFmt;
        out.getChildFile ("audio.wav").deleteFile();
        if (auto os = std::make_unique<FileOutputStream> (out.getChildFile ("audio.wav")); os->openedOk())
            if (std::unique_ptr<AudioFormatWriter> w (wavFmt.createWriterFor (os.get(), sr, 2, 16, {}, 0)); w != nullptr)
            {
                os.release();
                w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
            }

        return 0;
    }

    if (totalFrames <= 0)
    {
        // single poster: groove playing, chorus section
        clickButton ("Intro");
        for (int i = 0; i < 90; ++i) { runFrame(); pump (36); }
        clickButton ("Chorus");
        for (int i = 0; i < 70; ++i) { runFrame(); pump (36); }
        savePng (out.getChildFile ("poster.png"));

        clickButton ("MIXER");
        for (int i = 0; i < 20; ++i) { runFrame(); pump (36); }
        savePng (out.getChildFile ("poster_mixer.png"));

        auto setP = [&] (const char* id, float real)
        {
            if (auto* par = proc.apvts.getParameter (id))
                par->setValueNotifyingHost (par->convertTo0to1 (real));
        };
        for (auto* id : { "eqOn", "compOn", "satOn", "dlyOn" }) setP (id, 1.0f);
        setP ("eqLow", 4.0f); setP ("eqMid", -3.0f); setP ("eqHigh", 3.5f); setP ("roomMix", 0.22f);

        clickButton ("EFFECTS");
        for (int i = 0; i < 25; ++i) { runFrame(); pump (36); }
        savePng (out.getChildFile ("poster_fx.png"));

        clickButton ("GROOVE");
        for (int i = 0; i < 25; ++i) { runFrame(); pump (36); }
        savePng (out.getChildFile ("poster_groove.png"));
        return 0;
    }

    for (int f = 0; f < totalFrames; ++f)
    {
        if (f == 0)                 clickButton ("Intro");
        if (f == totalFrames / 3)   clickButton ("Verse");
        if (f == totalFrames * 2 / 3) { clickButton ("Chorus"); }
        if (f == totalFrames / 2)   { proc.loadKit (1); }          // switch kit while playing
        if (f == totalFrames * 5 / 6) { proc.loadKit (3); }

        runFrame();
        pump (36);
        savePng (out.getChildFile ("frame_" + String (f).paddedLeft ('0', 4) + ".png"));
    }

    WavAudioFormat wav;
    out.getChildFile ("audio.wav").deleteFile();
    if (auto os = std::make_unique<FileOutputStream> (out.getChildFile ("audio.wav")); os->openedOk())
        if (std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 16, {}, 0)); w != nullptr)
        {
            os.release();
            w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
        }

    return 0;
}
