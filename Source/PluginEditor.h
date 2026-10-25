#pragma once

#include "PluginProcessor.h"
#include "LookAndFeel.h"

//==============================================================================
// One mixer channel strip per pad (Mixer page)
class PadStrip : public juce::Component,
                 public juce::FileDragAndDropTarget
{
public:
    PadStrip (DrumForgeProcessor&, int padIndex);

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }

    void refresh();

private:
    void showMenu();
    void chooseFile();
    void updateName();

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    DrumForgeProcessor& proc;
    const int pad;
    juce::Colour accent;

    juce::TextButton nameButton, trigButton { "PLAY" }, muteButton { "M" },
                     soloButton { "S" }, gateButton { "GATE" };
    juce::Slider panSlider, faderSlider, tuneSlider;
    juce::ComboBox outBox, chokeBox;
    juce::Rectangle<int> tunerArea, tunerLabelArea;
    double detectedHz = 0.0;
    void detectPitch();

    juce::Rectangle<int> headerArea, panLabelArea, meterBounds, faderValueArea;
    juce::String lastName;
    float meter = 0.0f;
    bool dragOver = false;

    std::unique_ptr<juce::FileChooser> chooser;

    std::unique_ptr<SliderAtt> panAtt, faderAtt, tuneAtt;
    std::unique_ptr<ButtonAtt> muteAtt, soloAtt, gateAtt;
    std::unique_ptr<ComboAtt> outAtt, chokeAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadStrip)
};

//==============================================================================
// Clickable, animated drum kit (Performance page)
class DrumKitView : public juce::Component,
                    public juce::FileDragAndDropTarget
{
public:
    explicit DrumKitView (DrumForgeProcessor&);

    std::function<void (int)> onPadSelected;

    void setSelectedPad (int pad);
    int getSelectedPad() const { return selectedPad; }
    void refresh();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverPart = -1; repaint(); }

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    enum class Type { Kick, Snare, Tom, Cymbal, HiHat, Pedal };

    struct Part
    {
        Type type;
        const char* name;
        float cx, cy, rx, ry, bodyH, tilt;
        int pad, padShift, padAlt;
        float glow = 0.0f;
    };

    juce::Point<float> toDesign (juce::Point<float>) const;
    int hitTest (juce::Point<float> designPos, float* normDist = nullptr) const;
    int partForPad (int pad) const;

    void drawDrum (juce::Graphics&, const Part&) const;
    void drawKick (juce::Graphics&, const Part&) const;
    void drawCymbal (juce::Graphics&, const Part&, bool hat) const;
    void drawPole (juce::Graphics&, float x1, float y1, float x2, float y2, float w) const;
    void drawTripod (juce::Graphics&, float x, float y, float w) const;

    DrumForgeProcessor& proc;
    std::vector<Part> parts;
    int selectedPad = 2, hoverPart = -1;
    juce::AffineTransform xf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumKitView)
};

//==============================================================================
// Kit cards with search
class KitSelectorPanel : public juce::Component
{
public:
    explicit KitSelectorPanel (DrumForgeProcessor&);
    ~KitSelectorPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();                       // timer: follow current kit + new folders

    void stepKit (int delta);

private:
    class Grid;

    void openKitsFolder();

    DrumForgeProcessor& proc;
    juce::TextEditor search;
    juce::Viewport viewport;
    std::unique_ptr<Grid> grid;
    juce::TextButton kitsFolderButton { "MY KITS" }, importButton { "IMPORT" };
    std::unique_ptr<juce::FileChooser> chooser;
    int rescanCounter = 0;
    juce::String lastShownKit;
    size_t knownKits = 0;

    friend class DrumForgeEditor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KitSelectorPanel)
};

//==============================================================================
// Mixer & FX rack: edits the pad that is selected on the drum
class RackPanel : public juce::Component
{
public:
    explicit RackPanel (DrumForgeProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void bind (int pad);
    void refresh();

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void chooseFile();
    void showLoadMenu();

    DrumForgeProcessor& proc;
    int pad = 2;
    float meter = 0.0f;

    juce::Slider pitchSlider, decaySlider, panSlider, faderSlider, roomSizeSlider, roomMixSlider, humanizeSlider;
    juce::TextButton muteButton { "MUTE" }, soloButton { "SOLO" }, gateButton { "GATE" },
                     loadButton { "LOAD" }, clearButton { "CLEAR" };
    juce::ComboBox outBox, chokeBox;
    std::unique_ptr<juce::FileChooser> chooser;

    std::unique_ptr<SliderAtt> pitchAtt, decayAtt, panAtt, faderAtt, roomSizeAtt, roomMixAtt, humanizeAtt;
    std::unique_ptr<ButtonAtt> muteAtt, soloAtt, gateAtt;
    std::unique_ptr<ComboAtt> outAtt, chokeAtt;

    juce::Rectangle<int> titleArea, nameArea, meterArea, fxArea, knobLabelArea[3], fxLabelArea[3];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackPanel)
};

//==============================================================================
class MidiDragHandle : public juce::Component
{
public:
    explicit MidiDragHandle (DrumForgeProcessor& p) : proc (p) {}

    void setResource (const juce::String& r) { resource = r; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

private:
    DrumForgeProcessor& proc;
    juce::String resource;
    bool hover = false;
};

//==============================================================================
// Transport: genre -> groove -> section, play/stop, piano-roll overview of the MIDI
class GroovePanel : public juce::Component
{
public:
    explicit GroovePanel (DrumForgeProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

private:
    void fillGrooves();
    void buildSectionButtons();
    void selectSection (int index, bool play);
    void exportAll();

    DrumForgeProcessor& proc;
    const std::vector<GenreEntry>& lib;

    juce::ComboBox genreBox, grooveBox;
    juce::TextButton playButton { "PLAY" }, stopButton { "STOP" }, loopButton { "LOOP" },
                     syncButton { "HOST BPM" }, exportButton { "EXPORT ALL" };
    std::vector<std::unique_ptr<juce::TextButton>> sectionButtons;
    MidiDragHandle dragHandle;

    juce::Rectangle<int> genreLabel, grooveLabel, rollArea, bpmArea, titleArea;
    int currentSection = 0;
    juce::String currentResource;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GroovePanel)
};

//==============================================================================
// EFFECTS page: EQ, compressor, saturation, delay, reverb, output
class FxPanel : public juce::Component
{
public:
    explicit FxPanel (DrumForgeProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::String name;
        int card = 0;
        std::unique_ptr<SliderAtt> att;
        juce::Rectangle<int> labelArea;
    };

    struct Card
    {
        juce::String title, subtitle;
        juce::TextButton onButton { "ON" };
        std::unique_ptr<ButtonAtt> att;
        bool hasSwitch = false;
        juce::Rectangle<int> area, body;
        std::vector<Knob*> knobs;
    };

    Card& addCard (const juce::String& title, const juce::String& subtitle, const char* onId);
    void addKnob (Card&, const char* paramId, const juce::String& label, std::function<juce::String (double)> fmt);
    void drawEqCurve (juce::Graphics&, juce::Rectangle<float>) const;

    DrumForgeProcessor& proc;
    std::vector<std::unique_ptr<Card>> cards;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::Rectangle<int> eqArea, grArea;
    float grMeter = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxPanel)
};

//==============================================================================
// GROOVE page: browse the complete MIDI library (genre -> groove -> section)
class GrooveBrowser : public juce::Component,
                      public juce::FileDragAndDropTarget
{
public:
    explicit GrooveBrowser (DrumForgeProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    int importFiles (const juce::Array<juce::File>&);   // copies MIDI files into "My MIDI" and selects the first one

private:
    struct ListModel : public juce::ListBoxModel
    {
        std::function<int()> count;
        std::function<juce::String (int)> text;
        std::function<juce::String (int)> detail;
        std::function<bool (int)> isCurrent;
        std::function<void (int)> clicked;

        int getNumRows() override { return count ? count() : 0; }
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override { if (clicked) clicked (row); }
    };

    void reloadLibrary();
    void chooseMidiFiles();
    void rebuildGrooves();
    void pickGenre (int gi);
    void pickGroove (int row);
    void pickSection (int index, bool play);
    void exportAll();
    juce::String currentResource() const;

    DrumForgeProcessor& proc;
    std::vector<GenreEntry> lib;

    ListModel genreModel, grooveModel, sectionModel;
    juce::ListBox genreList, grooveList, sectionList;
    juce::TextEditor search;
    juce::TextButton playButton { "PLAY" }, stopButton { "STOP" }, loopButton { "LOOP" },
                     syncButton { "HOST BPM" }, exportButton { "EXPORT ALL" },
                     importButton { "IMPORT MIDI" }, gmButton { "GM MAP" };
    MidiDragHandle dragHandle;
    bool dropHighlight = false;

    std::vector<std::pair<int, int>> filtered;   // (genre, groove)
    int curGenre = 0, curGroove = 0, curSection = 0;
    int totalGrooves = 0, totalFiles = 0;

    juce::Rectangle<int> titleArea, statsArea, genreHead, grooveHead, sectionHead, bpmArea, rollArea;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrooveBrowser)
};

//==============================================================================
// KIT MIXER: tabbed 3x3 knob grid bound to the master tone / space parameters
class KitMixerPanel : public juce::Component
{
public:
    explicit KitMixerPanel (DrumForgeProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    struct K
    {
        juce::Slider s;
        juce::String name;
        int tab = 0;
        std::unique_ptr<SliderAtt> att;
        juce::Rectangle<int> labelArea;
    };

    void addKnob (int tab, const char* id, const juce::String& name, std::function<juce::String (double)> fmt, const char* fxOnId = nullptr);
    void showTab (int t);

    DrumForgeProcessor& proc;
    std::vector<std::unique_ptr<K>> knobs;
    juce::TextButton toneTab { "TONE" }, spaceTab { "SPACE" };
    int tab = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KitMixerPanel)
};

//==============================================================================
// PAD SHAPER: tune / decay / pan / level of the pad selected on the kit, plus load & clear
class PadShaperPanel : public juce::Component
{
public:
    explicit PadShaperPanel (DrumForgeProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void bind (int pad);
    void refresh();
    void showLoadMenu();

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    void chooseFile();

    DrumForgeProcessor& proc;
    int pad = 2;
    juce::Slider tuneS, decayS, panS, levelS;
    juce::TextButton muteB { "MUTE" }, soloB { "SOLO" }, gateB { "GATE" }, loadB { "LOAD" }, clearB { "CLEAR" };
    std::unique_ptr<SliderAtt> tuneA, decayA, panA, levelA;
    std::unique_ptr<ButtonAtt> muteA, soloA, gateA;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> nameArea, labelArea[4];
    juce::String lastSample;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadShaperPanel)
};

//==============================================================================
// Numbered list of the 16 pads with their samples
class PadListPanel : public juce::Component,
                     private juce::ListBoxModel
{
public:
    explicit PadListPanel (DrumForgeProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setSelected (int pad);
    void refresh();

    std::function<void (int)> onPick, onDoubleClick;

private:
    int getNumRows() override { return df::numPads; }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override { if (onPick) onPick (row); }
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick (row); }

    DrumForgeProcessor& proc;
    juce::ListBox list;
    int selected = 2;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadListPanel)
};

//==============================================================================
// PERCUSSION MIXER: group faders (kick, snare, toms, hats, cymbals) that move every pad of the group
class PercMixerPanel : public juce::Component
{
public:
    explicit PercMixerPanel (DrumForgeProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

private:
    struct Grp
    {
        juce::String name;
        std::vector<int> pads;
        juce::Slider fader, pan;
        juce::Rectangle<int> nameArea, dbArea, meterArea;
        float meter = 0.0f;
    };

    void pushToPads (Grp&, const char* base, double value);

    DrumForgeProcessor& proc;
    std::vector<std::unique_ptr<Grp>> groups;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PercMixerPanel)
};

//==============================================================================
class DrumForgeEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit DrumForgeEditor (DrumForgeProcessor&);
    ~DrumForgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPage (int page);
    void showMidiMapMenu();

    DrumForgeProcessor& proc;

    DblLookAndFeel laf;
    juce::Image logo, logoDbl, logoSkm;
    juce::Rectangle<int> logoDblArea, logoSkmArea;

    juce::TextButton prevKitButton { "<" }, nextKitButton { ">" },
                     performanceTab { "PERFORMANCE" }, mixerTab { "MIXER" },
                     fxTab { "EFFECTS" }, grooveTab { "GROOVE" };
    juce::Slider masterSlider;
    juce::Slider roomSizeSlider, roomMixSlider, humanizeSlider;

    KitSelectorPanel kitPanel;
    DrumKitView drumView;
    KitMixerPanel kitMixer;
    PadShaperPanel shaper;
    PadListPanel padList;
    PercMixerPanel percMixer;
    juce::TextButton midiMapButton { "MIDI MAP" };
    juce::TextButton activateButton { "ACTIVATE" };
    void showActivateDialog();
    void updateActivateButton();
    GroovePanel groovePanel;
    FxPanel fxPanel;
    GrooveBrowser grooveBrowser;
    std::array<std::unique_ptr<PadStrip>, df::numPads> strips;

    // mixer console (Mixer page)
    static constexpr int numGroups = 5;
    std::array<juce::TextButton, numGroups> groupMute, groupSolo;
    juce::TextButton clearMutesButton { "CLEAR MUTES" }, clearSolosButton { "CLEAR SOLOS" },
                     resetFadersButton { "RESET FADERS" }, resetPansButton { "RESET PANS" };
    juce::TextButton saveMixButton { "SAVE MIX" }, loadMixButton { "LOAD MIX" };
    std::array<juce::Slider, 4> micSlider;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> micAtt;
    std::array<juce::Rectangle<int>, 4> micLabelArea;
    juce::Slider sensSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensAtt;
    juce::Rectangle<int> sensLabelArea, micBlockArea;
    void saveMixDialog();
    void showMixMenu (juce::Component* target = nullptr);
    juce::TextButton presetsButton { "PRESETS" };
    juce::String currentPreset, currentPresetKit;
    void updatePresetButton();
    juce::Rectangle<int> consoleArea, consoleTitleArea, deckArea, deckGroupsArea;
    std::array<juce::Rectangle<int>, numGroups> groupBands, groupBlocks;
    void setPadsBool (const juce::String& param, int first, int last, bool value);
    void resetPadsParam (const juce::String& param);
    bool allPadsBool (const juce::String& param, int first, int last) const;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAtt> masterAtt, roomSizeAtt, roomMixAtt, humanizeAtt;

    int page = 0;
    float meterL = 0.0f, meterR = 0.0f;
    juce::String lastKitName, lastMapCaption;

    juce::Rectangle<int> plaqueArea, logoArea, kitNavArea, midiMapCaption, topRowArea, footerArea, masterMeterArea,
                         masterLabelArea, roomLabelArea, mixLabelArea, humLabelArea, stageArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumForgeEditor)
};
