#include "PluginEditor.h"
#include <BinaryData.h>

using namespace juce;

namespace
{
    Colour accentForPad (int pad)
    {
        if (pad <= 1)  return Colour (0xffd88a3a);   // kicks      - gold
        if (pad <= 4)  return Colour (0xffe8963a);   // snares     - amber
        if (pad <= 8)  return Colour (0xffd0623a);   // toms       - copper
        if (pad <= 11) return Colour (0xffefb997);   // hi-hats    - silver
        return Colour (0xfff1c88c);                  // cymbals    - pale gold
    }

    struct GroupInfo { const char* name; int first, last; Colour tint; };
    const GroupInfo kGroups[5] = {
        { "KICK",    0,  1,  Colour (0xff2f6fc4) },
        { "SNARE",   2,  4,  Colour (0xff1f9aa6) },
        { "TOMS",    5,  8,  Colour (0xff8da12a) },
        { "HATS",    9,  11, Colour (0xffdba421) },
        { "CYMBALS", 12, 20, Colour (0xffc4392f) } };

    int groupOfPad (int pad)
    {
        for (int i = 0; i < 5; ++i)
            if (pad >= kGroups[i].first && pad <= kGroups[i].last)
                return i;
        return 4;
    }

    const char* shortPadName (int pad)
    {
        static const char* n[21] = { "KICK", "KICK 2", "SNARE", "RIM", "STICK", "FLOOR", "LOW", "MID", "HIGH", "HH CL",
                                     "HH PED", "HH OP", "CRASH 1", "SPLASH", "RIDE", "BELL", "CRASH 2", "CRASH 3",
                                     "SPLSH 2", "CHINA", "FX" };
        return n[jlimit (0, 20, pad)];
    }

    void drawSegmentMeter (Graphics& g, Rectangle<int> area, float level, Colour accent)
    {
        g.setColour (Colour (0xff070707));
        g.fillRoundedRectangle (area.toFloat(), 2.0f);

        const int n = jmax (4, area.getHeight() / 4);
        const float segH = (float) area.getHeight() / (float) n;
        const float db = Decibels::gainToDecibels (level, -60.0f);

        for (int i = 0; i < n; ++i)
        {
            const float segDb = jmap ((float) i + 0.5f, 0.0f, (float) n, -60.0f, 6.0f);
            const bool lit = segDb < db;
            const float y = (float) area.getBottom() - (float) (i + 1) * segH;
            const Rectangle<float> r ((float) area.getX() + 1.0f, y + 0.5f, (float) area.getWidth() - 2.0f, segH - 1.0f);

            Colour c = segDb > 0.0f ? theme::red : (segDb > -9.0f ? theme::goldLight : accent);
            g.setColour (lit ? c : c.withAlpha (0.12f));
            g.fillRect (r);
        }
    }

    void drawPanel (Graphics& g, Rectangle<float> r, float radius = 7.0f)
    {
        // dark carved panel with copper frame and bright corner brackets
        g.setGradientFill (ColourGradient (Colour (0xff261910), 0.0f, r.getY(), Colour (0xff0f0905), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        g.setGradientFill (ColourGradient (Colour (0xffe8923a).withAlpha (0.10f), r.getCentreX(), r.getY(), Colours::transparentBlack,
                                           r.getCentreX(), r.getY() + r.getHeight() * 0.6f, true));
        g.fillRoundedRectangle (r, radius);

        g.setColour (Colour (0xffb8651f));
        g.drawRoundedRectangle (r, radius, 1.5f);
        g.setColour (Colour (0xffe8923a).withAlpha (0.22f));
        g.drawRoundedRectangle (r.reduced (3.5f), radius - 2.0f, 0.8f);

        const float L = 11.0f;
        g.setColour (Colour (0xfff1b070));
        for (int cx = 0; cx < 2; ++cx)
            for (int cy = 0; cy < 2; ++cy)
            {
                const float x = cx ? r.getRight() - 1.5f : r.getX() + 1.5f, y = cy ? r.getBottom() - 1.5f : r.getY() + 1.5f;
                const float dx = cx ? -L : L, dy = cy ? -L : L;
                g.drawLine (x, y, x + dx, y, 2.0f);
                g.drawLine (x, y, x, y + dy, 2.0f);
            }
    }

    void drawPanelTitle (Graphics& g, Rectangle<int> r, const String& text)
    {
        g.setColour (Colour (0xfff7e1c4));
        g.setFont (Font (FontOptions (12.5f, Font::bold)).withExtraKerningFactor (0.12f));
        g.drawText (text, r, Justification::centred, false);

        const auto ic = Rectangle<float> ((float) r.getRight() - 24.0f, (float) r.getCentreY() - 6.5f, 13.0f, 13.0f);
        g.setColour (Colour (0xffb8651f));
        g.drawEllipse (ic, 1.1f);
        g.setFont (Font (FontOptions (9.0f, Font::bold)));
        g.drawText ("i", ic, Justification::centred, false);
    }

    // "Swap with a kit sample" sub-menu: every embedded sample, grouped by kit, loadable on any pad
    const std::vector<FactorySampleInfo>& swapList()
    {
        // only kit pieces (kick, snare, side stick, toms) can be swapped - the cymbals are the same in every kit
        static const auto list = []
        {
            auto all = DrumForgeProcessor::getFactorySamples();
            all.erase (std::remove_if (all.begin(), all.end(), [] (const FactorySampleInfo& f) { return f.pad >= 9; }), all.end());
            return all;
        }();
        return list;
    }

    // quick section: the same piece (same pad) from every kit
    void addSamePieceItems (PopupMenu& root, int firstId, int pad)
    {
        if (pad >= 9)
        {
            root.addItem (-1, "Cymbals are the same in every kit", false);
            return;
        }

        const auto& list = swapList();
        bool any = false;
        for (size_t i = 0; i < list.size(); ++i)
            if (list[i].pad == pad && list[i].kitIndex < 3)
            {
                root.addItem (firstId + (int) i, list[i].name + "  -  " + list[i].kit);
                any = true;
            }

        if (any)
            root.addSeparator();
    }

    void addSwapMenu (PopupMenu& root, int firstId)
    {
        PopupMenu all, cur;
        String kit;

        auto flush = [&]
        {
            if (kit.isNotEmpty())
                all.addSubMenu (kit, cur);
        };

        const auto& list = swapList();
        for (size_t i = 0; i < list.size(); ++i)
        {
            if (list[i].kit != kit)
            {
                flush();
                cur = PopupMenu();
                kit = list[i].kit;
            }

            cur.addItem (firstId + (int) i, list[i].name);
        }

        flush();
        if (! list.empty())
            root.addSubMenu ("Swap with a kit sample", all);
    }

    void label (Graphics& g, const String& text, Rectangle<int> r, float size = 10.0f,
                Colour c = theme::textDim, Justification j = Justification::centred)
    {
        g.setColour (c);
        g.setFont (Font (FontOptions (size, Font::bold)));
        g.drawText (text, r, j, false);
    }
}

//==============================================================================
PadStrip::PadStrip (DrumForgeProcessor& p, int padIndex)
    : proc (p), pad (padIndex), accent (accentForPad (padIndex))
{
    auto& apvts = proc.apvts;

    nameButton.onClick = [this] { showMenu(); };
    nameButton.setTooltip ("Click to load / clear a sample, or drop an audio file here");
    addAndMakeVisible (nameButton);

    trigButton.onClick = [this] { proc.triggerPad (pad); };
    addAndMakeVisible (trigButton);

    muteButton.setClickingTogglesState (true);
    muteButton.setColour (TextButton::buttonOnColourId, theme::red);
    addAndMakeVisible (muteButton);

    soloButton.setClickingTogglesState (true);
    soloButton.setColour (TextButton::buttonOnColourId, Colour (0xffd97d1f));
    soloButton.setColour (TextButton::textColourOnId, Colours::black);
    addAndMakeVisible (soloButton);

    gateButton.setClickingTogglesState (true);
    gateButton.setColour (TextButton::buttonOnColourId, theme::gold);
    gateButton.setColour (TextButton::textColourOnId, Colours::black);
    gateButton.setTooltip ("Gate: the sound stops when the key is released");
    addAndMakeVisible (gateButton);

    auto setupKnob = [this] (Slider& s, double def)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::TextBoxBelow, false, 46, 12);
        s.setTextBoxIsEditable (false);
        s.setDoubleClickReturnValue (true, def);
        addAndMakeVisible (s);
    };

    setupKnob (panSlider, 0.0);

    faderSlider.setSliderStyle (Slider::LinearVertical);
    faderSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    faderSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (faderSlider);

    auto fill = [&apvts] (ComboBox& box, const String& id)
    {
        if (auto* c = dynamic_cast<AudioParameterChoice*> (apvts.getParameter (id)))
            box.addItemList (c->choices, 1);
    };

    fill (outBox, df::pid ("out", pad));
    fill (chokeBox, df::pid ("choke", pad));
    outBox.setTooltip ("Output bus for this pad");
    chokeBox.setTooltip ("Choke group: pads in the same group cut each other");
    addAndMakeVisible (outBox);
    addAndMakeVisible (chokeBox);

    panAtt   = std::make_unique<SliderAtt> (apvts, df::pid ("pan", pad), panSlider);
    faderAtt = std::make_unique<SliderAtt> (apvts, df::pid ("gain", pad), faderSlider);
    muteAtt  = std::make_unique<ButtonAtt> (apvts, df::pid ("mute", pad), muteButton);
    soloAtt  = std::make_unique<ButtonAtt> (apvts, df::pid ("solo", pad), soloButton);
    gateAtt  = std::make_unique<ButtonAtt> (apvts, df::pid ("gate", pad), gateButton);
    outAtt   = std::make_unique<ComboAtt>  (apvts, df::pid ("out", pad), outBox);
    chokeAtt = std::make_unique<ComboAtt>  (apvts, df::pid ("choke", pad), chokeBox);

    // The attachments install their own text conversion, so ours must be set afterwards
    panSlider.textFromValueFunction = [] (double v)
    {
        if (std::abs (v) < 0.005)
            return String ("C");
        return String (roundToInt (std::abs (v) * 100.0)) + (v < 0 ? " L" : " R");
    };
    panSlider.updateText();

    tuneSlider.setSliderStyle (Slider::LinearHorizontal);
    tuneSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    tuneSlider.setDoubleClickReturnValue (true, 0.0);
    tuneSlider.setTooltip ("TUNER: tune this piece (+/-24 semitones, fine steps of 1 cent). Double-click = original pitch");
    addAndMakeVisible (tuneSlider);
    tuneAtt = std::make_unique<SliderAtt> (apvts, df::pid ("pitch", pad), tuneSlider);
    tuneSlider.onValueChange = [this] { repaint (tunerArea); };

    faderSlider.onValueChange = [this] { repaint (faderValueArea); };
    muteButton.setColour (TextButton::buttonOnColourId, Colour (0xffd23b2e));
    nameButton.setColour (TextButton::buttonColourId, Colour (0xff15100c));

    updateName();
}

// Fundamental frequency of the loaded sample (autocorrelation on the body of the hit); 0 = unpitched / unknown
void PadStrip::detectPitch()
{
    detectedHz = 0.0;
    auto s = proc.peekSample (pad);
    if (s == nullptr)
        return;

    for (int i = 0; i < 4 && s->data.getNumSamples() == 0; ++i)   // cymbal sets / layers: use the first available audio
    {
        if (! s->layers.empty()) s = s->layers.back();
        else if (s->mics[1] != nullptr) s = s->mics[1];
        else break;
    }

    const int n = s->data.getNumSamples();
    if (n < 2048 || pad >= 9)      // only the drum pieces are tuned by ear
        return;

    const double sr = s->sampleRate;
    const int start = jmin (n - 4096, (int) (0.012 * sr));
    const int win = jmin (n - start, (int) (0.09 * sr));
    const int minLag = (int) (sr / 1000.0), maxLag = jmin (win / 2, (int) (sr / 30.0));
    const float* d = s->data.getReadPointer (0) + start;

    double e0 = 0.0;
    for (int i = 0; i < win - maxLag; ++i)
        e0 += (double) d[i] * d[i];
    if (e0 < 1.0e-6)
        return;

    double best = 0.0;
    int bestLag = 0;
    std::vector<double> r ((size_t) maxLag + 2, 0.0);
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double acc = 0.0, e1 = 0.0;
        for (int i = 0; i < win - maxLag; ++i)
        {
            acc += (double) d[i] * d[i + lag];
            e1  += (double) d[i + lag] * d[i + lag];
        }
        r[(size_t) lag] = acc / std::sqrt (e0 * e1 + 1.0e-12);
    }
    for (int lag = minLag + 1; lag < maxLag; ++lag)       // first strong local peak (avoids octave errors)
        if (r[(size_t) lag] > 0.55 && r[(size_t) lag] >= r[(size_t) lag - 1] && r[(size_t) lag] >= r[(size_t) lag + 1])
        {
            bestLag = lag; best = r[(size_t) lag]; break;
        }
    if (bestLag == 0)
        return;

    const double a = r[(size_t) bestLag - 1], b = r[(size_t) bestLag], c = r[(size_t) bestLag + 1];
    const double den = a - 2.0 * b + c;
    const double off = std::abs (den) > 1.0e-9 ? 0.5 * (a - c) / den : 0.0;
    juce::ignoreUnused (best);
    detectedHz = sr / ((double) bestLag + off);
}

void PadStrip::updateName()
{
    lastName = proc.getSampleName (pad);
    detectPitch();
    repaint (tunerArea);
    nameButton.setButtonText (lastName.isEmpty() ? String ("empty") : lastName);
    nameButton.setTooltip (lastName.isEmpty() ? String ("Empty - click to load a sample, or drop an audio file here")
                                              : lastName + "  (click to change)");
    nameButton.setColour (TextButton::textColourOffId, lastName.isEmpty() ? theme::textDim : Colours::white);
}

void PadStrip::refresh()
{
    const float lv = proc.padLevel[(size_t) pad].load();
    const float newMeter = jmax (lv, meter * 0.88f);

    if (std::abs (newMeter - meter) > 0.0005f)
    {
        meter = newMeter;
        repaint (meterBounds);
        repaint (headerArea);
    }

    if (proc.getSampleName (pad) != lastName)
        updateName();
}

void PadStrip::paint (Graphics& g)
{
    const auto tint = kGroups[groupOfPad (pad)].tint;
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    // glass channel body tinted by group colour (console look)
    g.setGradientFill (ColourGradient (tint.withAlpha (0.50f), 0.0f, r.getY(),
                                       tint.darker (0.9f).withAlpha (0.22f), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Colour (0xff070605).withAlpha (0.40f));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (tint.withAlpha (0.75f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    // header: lights up on hit
    const float glow = jlimit (0.0f, 1.0f, meter * 2.2f);
    auto h = headerArea.toFloat();
    g.setGradientFill (ColourGradient (tint.brighter (0.2f + 0.5f * glow), 0.0f, h.getY(),
                                       tint.darker (0.5f), 0.0f, h.getBottom(), false));
    g.fillRoundedRectangle (h, 2.5f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (h, 2.5f, 1.0f);

    auto hi = headerArea;
    label (g, String (pad + 1), hi.removeFromTop (12), 9.0f, Colours::white.withAlpha (0.85f));
    label (g, shortPadName (pad), hi.removeFromTop (14), 10.5f, Colours::white);
    label (g, MidiMessage::getMidiNoteName (df::firstNote + pad, true, true, 3), hi, 8.5f, Colours::white.withAlpha (0.7f));

    if (dragOver)
    {
        g.setColour (theme::gold);
        g.drawRoundedRectangle (r, 3.0f, 2.0f);
        g.setColour (theme::gold.withAlpha (0.14f));
        g.fillRoundedRectangle (r, 3.0f);
    }

    label (g, "PAN", panLabelArea, 8.5f, Colour (0xffe8c9a0));

    // tuner readout: detected note of the (re-tuned) sample + cents
    {
        auto t = tunerArea.toFloat();
        g.setColour (Colour (0xff050403));
        g.fillRoundedRectangle (t, 2.0f);
        g.setColour (Colour (0xff3d7a4a));
        g.drawRoundedRectangle (t, 2.0f, 1.0f);
        const double st = tuneSlider.getValue();
        String txt = String (st >= 0 ? "+" : "") + String (st, 2) + " st";
        Colour col = Colour (0xfff2d28a);
        if (detectedHz > 0.0)
        {
            const double hz = detectedHz * std::pow (2.0, st / 12.0);
            const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
            const int nn = roundToInt (midi);
            const int cents = roundToInt ((midi - nn) * 100.0);
            txt = MidiMessage::getMidiNoteName (nn, true, true, 3) + (cents >= 0 ? " +" : " ") + String (cents) + "c";
            col = std::abs (cents) <= 5 ? Colour (0xff7bff9a) : std::abs (cents) <= 20 ? Colour (0xfff2d28a) : Colour (0xffff8a6a);
        }
        auto ta = tunerArea;
        if (detectedHz > 0.0)
        {
            // cents needle: centre = in tune
            const double hz = detectedHz * std::pow (2.0, st / 12.0);
            const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
            const float c = (float) jlimit (-0.5, 0.5, midi - roundToInt (midi));
            auto bar = ta.removeFromBottom (5).toFloat().reduced (3.0f, 1.0f);
            g.setColour (Colour (0xff2a211a));
            g.fillRect (bar);
            g.setColour (Colours::white.withAlpha (0.5f));
            g.fillRect (bar.getCentreX() - 0.5f, bar.getY(), 1.0f, bar.getHeight());
            g.setColour (col);
            g.fillRect (bar.getCentreX() + c * bar.getWidth() - 1.5f, bar.getY() - 1.0f, 3.0f, bar.getHeight() + 2.0f);
        }
        label (g, txt, ta, 11.0f, col);
        label (g, "TUNER", tunerLabelArea, 8.5f, Colour (0xff9fe8b0));
    }

    // fader value readout (dB)
    g.setColour (Colour (0xff050403));
    g.fillRoundedRectangle (faderValueArea.toFloat().reduced (3.0f, 1.0f), 2.0f);
    const double v = faderSlider.getValue();
    label (g, v <= -99.0 ? String ("-inf") : String (v, 1), faderValueArea, 9.5f, Colour (0xfff2d28a));

    drawSegmentMeter (g, meterBounds, meter, tint.brighter (0.3f));
}

void PadStrip::resized()
{
    auto r = getLocalBounds().reduced (3, 3);

    headerArea = r.removeFromTop (40);
    r.removeFromTop (3);
    nameButton.setBounds (r.removeFromTop (17));
    r.removeFromTop (2);
    trigButton.setBounds (r.removeFromTop (17));
    r.removeFromTop (3);

    panLabelArea = r.removeFromTop (10);
    panSlider.setBounds (r.removeFromTop (42));
    r.removeFromTop (2);
    tunerLabelArea = r.removeFromTop (11);
    tunerArea = r.removeFromTop (22);
    r.removeFromTop (2);
    tuneSlider.setBounds (r.removeFromTop (18));
    r.removeFromTop (4);

    auto row = r.removeFromTop (20);
    const int bw = row.getWidth() / 2;
    muteButton.setBounds (row.removeFromLeft (bw).reduced (1, 0));
    soloButton.setBounds (row.reduced (1, 0));
    r.removeFromTop (3);

    outBox.setBounds (r.removeFromTop (17));
    r.removeFromTop (2);
    chokeBox.setBounds (r.removeFromTop (17));
    r.removeFromTop (2);
    gateButton.setBounds (r.removeFromTop (17));
    r.removeFromTop (4);

    faderValueArea = r.removeFromBottom (15);
    r.removeFromBottom (2);
    meterBounds = r.removeFromRight (9).reduced (0, 3);
    r.removeFromRight (2);
    faderSlider.setBounds (r);
}

bool PadStrip::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (proc.isSupportedFile (File (f)))
            return true;

    return false;
}

void PadStrip::filesDropped (const StringArray& files, int, int)
{
    dragOver = false;

    int target = pad;
    for (auto& path : files)
    {
        if (target >= df::numPads)
            break;

        const File f (path);
        if (proc.isSupportedFile (f) && proc.loadSample (target, f))
            ++target;
    }

    repaint();
}

void PadStrip::showMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Swap this piece with:");
    addSamePieceItems (m, 1000, pad);
    if (pad < 9)
        addSwapMenu (m, 1000);
    if (df::externalSamplesEnabled)
        m.addItem (1, "Load sample from file...");
    m.addSeparator();
    m.addItem (2, "Clear", proc.getSampleName (pad).isNotEmpty());

    Component::SafePointer<PadStrip> safe (this);

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&nameButton), [safe] (int result)
    {
        if (safe == nullptr)
            return;

        if (result == 1)
            safe->chooseFile();
        else if (result == 2)
            safe->proc.clearSample (safe->pad);
        else if (result >= 1000)
            safe->proc.loadFactorySample (safe->pad, swapList()[(size_t) (result - 1000)].resource);
    });
}

void PadStrip::chooseFile()
{
    chooser = std::make_unique<FileChooser> ("Choose a sample for pad " + String (pad + 1),
                                             proc.lastDirectory,
                                             "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

    Component::SafePointer<PadStrip> safe (this);

    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe] (const FileChooser& fc)
    {
        if (safe == nullptr)
            return;

        const auto f = fc.getResult();
        if (f.existsAsFile())
            safe->proc.loadSample (safe->pad, f);
    });
}

//==============================================================================
void MidiDragHandle::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool enabled = resource.isNotEmpty();

    g.setGradientFill (ColourGradient ((hover && enabled ? theme::gold : theme::goldDark).withAlpha (0.9f), 0.0f, r.getY(),
                                       theme::goldDark.darker (0.6f), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (theme::gold);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    // grip dots
    g.setColour (Colours::black.withAlpha (0.75f));
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
            g.fillEllipse (r.getX() + 9.0f + (float) col * 5.0f, r.getCentreY() - 7.0f + (float) row * 6.0f, 3.0f, 3.0f);

    g.setColour (Colours::black);
    g.setFont (Font (FontOptions (11.5f, Font::bold)));
    g.drawText ("DRAG MIDI TO DAW", r.withTrimmedLeft (26), Justification::centredLeft, false);
}

void MidiDragHandle::mouseDrag (const MouseEvent& e)
{
    if (resource.isEmpty() || e.getDistanceFromDragStart() < 6)
        return;

    const auto folder = File::getSpecialLocation (File::tempDirectory).getChildFile ("African Drum MIDI");
    const auto f = proc.writeGrooveFile (resource, folder);

    if (f.existsAsFile())
        DragAndDropContainer::performExternalDragDropOfFiles (StringArray { f.getFullPathName() }, false, this);
}

//==============================================================================
namespace
{
    Colour kitTint (const KitInfo& k)
    {
        if (k.factoryIndex == 0) return Colour (0xffee9a3c);   // Studio Maple - gold
        if (k.factoryIndex == 1) return Colour (0xffffd5ae);   // Rock Birch   - silver
        if (k.factoryIndex == 2) return Colour (0xffe8743f);   // Jazz Bop     - copper
        if (k.factoryIndex == 3) return Colour (0xff4fa6ee);   // Tight Funk   - blue
        if (k.factoryIndex == 4) return Colour (0xffe85a4f);   // LIVE DRUM    - red
        if (k.factoryIndex == 5) return Colour (0xff4fd08a);   // KIT DRUM     - green
        if (k.factoryIndex == 6) return Colour (0xffb575f0);   // KIT 2        - violet
        return Colour::fromHSV ((float) (k.name.hashCode() & 0xffff) / 65535.0f, 0.55f, 0.95f, 1.0f);
    }

    // Small stylised kit used on the kit cards
    void drawMiniKit (Graphics& g, Rectangle<float> a, Colour tint)
    {
        const float w = a.getWidth(), h = a.getHeight();
        auto at = [&] (float x, float y) { return Point<float> (a.getX() + x * w, a.getY() + y * h); };

        auto cymbal = [&] (float x, float y, float rx)
        {
            const auto c = at (x, y);
            g.setGradientFill (ColourGradient (Colour (0xff9c5719), c.x - rx * w, c.y, Colour (0xffffd9a3), c.x, c.y, false));
            g.fillEllipse (c.x - rx * w, c.y - 0.03f * h, rx * 2.0f * w, 0.07f * h);
            g.setColour (Colour (0xff5a330f));
            g.drawLine (c.x, c.y, c.x, a.getBottom() - 0.12f * h, 1.0f);
        };

        cymbal (0.17f, 0.20f, 0.14f);
        cymbal (0.85f, 0.16f, 0.14f);
        cymbal (0.08f, 0.50f, 0.10f);

        auto drum = [&] (float x, float y, float rx, float ry, float bodyH)
        {
            const auto c = at (x, y);
            g.setGradientFill (ColourGradient (tint.darker (0.4f), c.x - rx * w, 0.0f, tint.brighter (0.4f), c.x, 0.0f, false));
            g.fillRect (c.x - rx * w, c.y, rx * 2.0f * w, bodyH * h);
            g.fillEllipse (c.x - rx * w, c.y + bodyH * h - ry * h, rx * 2.0f * w, ry * 2.0f * h);
            g.setColour (Colour (0xfff6f2e6));
            g.fillEllipse (c.x - rx * w, c.y - ry * h, rx * 2.0f * w, ry * 2.0f * h);
            g.setColour (tint.brighter (0.2f));
            g.drawEllipse (c.x - rx * w, c.y - ry * h, rx * 2.0f * w, ry * 2.0f * h, 1.2f);
        };

        // kick
        const auto kc = at (0.50f, 0.66f);
        const float kr = 0.27f * jmin (w, h * 1.4f);
        g.setGradientFill (ColourGradient (tint.brighter (0.3f), kc.x, kc.y - kr, tint.darker (0.5f), kc.x, kc.y + kr, false));
        g.fillEllipse (kc.x - kr, kc.y - kr, kr * 2.0f, kr * 2.0f);
        g.setGradientFill (ColourGradient (Colour (0xfffff1de), kc.x, kc.y, Colour (0xffc6c0b0), kc.x + kr, kc.y + kr, true));
        g.fillEllipse (kc.x - kr * 0.82f, kc.y - kr * 0.82f, kr * 1.64f, kr * 1.64f);

        drum (0.38f, 0.40f, 0.10f, 0.045f, 0.07f);
        drum (0.62f, 0.40f, 0.10f, 0.045f, 0.07f);
        drum (0.22f, 0.58f, 0.12f, 0.05f, 0.06f);
        drum (0.80f, 0.62f, 0.12f, 0.05f, 0.10f);
    }
}

void drawGrooveRoll (Graphics& g, DrumForgeProcessor& proc, Rectangle<int> area, const String& resource)
{
    // ---- MIDI overview (piano roll)
    auto roll = area;
    auto head = roll.removeFromTop (16);
    label (g, "MIDI MAPPING OVERVIEW", head.withTrimmedLeft (4), 10.0f, theme::goldLight, Justification::centredLeft);
    if (! resource.isEmpty())
        label (g, DrumForgeProcessor::getGrooveDisplayName (resource), head.withTrimmedRight (4), 10.0f,
               theme::textDim, Justification::centredRight);

    g.setGradientFill (ColourGradient (Colour (0xff17110e), 0.0f, (float) roll.getY(), Colour (0xff281f19), 0.0f, (float) roll.getBottom(), false));
    g.fillRoundedRectangle (roll.toFloat(), 5.0f);
    g.setColour (Colour (0xff050709));
    g.drawRoundedRectangle (roll.toFloat(), 5.0f, 1.0f);

    static const char* laneNames[6] = { "KICK", "SNARE", "TOMS", "HI-HAT", "CRASH", "RIDE" };
    static const int lanePad[16] = { 0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 5, 5 };
    static const int laneFirstPad[6] = { 0, 2, 5, 9, 12, 14 };

    auto inner = roll.reduced (2);
    const int labelW = 62;
    auto labels = inner.removeFromLeft (labelW);
    const float laneH = (float) inner.getHeight() / 6.0f;

    for (int l = 0; l < 6; ++l)
    {
        auto lr = Rectangle<float> ((float) inner.getX(), (float) inner.getY() + laneH * (float) l, (float) inner.getWidth(), laneH);
        if (l % 2 == 0)
        {
            g.setColour (Colours::white.withAlpha (0.035f));
            g.fillRect (lr);
        }

        label (g, laneNames[l], Rectangle<int> (labels.getX() + 6, (int) lr.getY(), labelW - 6, (int) laneH), 9.0f,
               accentForPad (laneFirstPad[l]), Justification::centredLeft);
    }

    const GrooveSeq* seq = resource.isEmpty() ? nullptr : proc.loadGrooveSeq (resource);

    if (seq != nullptr)
    {
        const int beats = (int) std::round (seq->lengthBeats);
        for (int b = 0; b <= beats; ++b)
        {
            const float x = (float) inner.getX() + (float) inner.getWidth() * (float) b / (float) beats;
            g.setColour (Colours::white.withAlpha (b % 4 == 0 ? 0.20f : 0.06f));
            g.fillRect (x, (float) inner.getY(), 1.0f, (float) inner.getHeight());
        }

        const float noteW = jmax (3.0f, (float) inner.getWidth() / (float) beats * 0.16f);

        for (const auto& e : seq->events)
        {
            if (! e.msg.isNoteOn())
                continue;

            const int pad = e.msg.getNoteNumber() - df::firstNote;
            if (! isPositiveAndBelow (pad, 16))
                continue;

            const int lane = lanePad[pad];
            const float x = (float) inner.getX() + (float) inner.getWidth() * (float) (e.beat / seq->lengthBeats);
            const float y = (float) inner.getY() + laneH * (float) lane + 2.0f;

            g.setColour (accentForPad (pad).withAlpha (0.45f + 0.55f * e.msg.getFloatVelocity()));
            g.fillRoundedRectangle (x, y, noteW, laneH - 4.0f, 1.5f);
        }

        if (proc.isGroovePlaying())
        {
            const float x = (float) inner.getX() + (float) inner.getWidth() * proc.getGrooveProgress();
            g.setColour (theme::gold.withAlpha (0.25f));
            g.fillRect (x - 3.0f, (float) inner.getY(), 7.0f, (float) inner.getHeight());
            g.setColour (theme::goldLight);
            g.fillRect (x - 0.5f, (float) inner.getY(), 2.0f, (float) inner.getHeight());

            Path tri;
            tri.addTriangle (x - 5.0f, (float) roll.getY() - 1.0f, x + 6.0f, (float) roll.getY() - 1.0f, x + 0.5f, (float) roll.getY() + 6.0f);
            g.setColour (theme::gold);
            g.fillPath (tri);
        }
    }
    else
    {
        label (g, "Choose a genre and a groove, then press PLAY", inner, 11.0f, theme::textDim);
    }
}

//==============================================================================
GroovePanel::GroovePanel (DrumForgeProcessor& p)
    : proc (p), lib (DrumForgeProcessor::getGrooveLibrary()), dragHandle (p)
{
    addAndMakeVisible (genreBox);
    addAndMakeVisible (grooveBox);

    int id = 1;
    for (const auto& g : lib)
        genreBox.addItem (g.name, id++);

    genreBox.onChange = [this] { fillGrooves(); };
    grooveBox.onChange = [this] { buildSectionButtons(); };

    playButton.setColour (TextButton::buttonColourId, Colour (0xff2f9e5b));
    playButton.setColour (TextButton::buttonOnColourId, Colour (0xff45d27a));
    playButton.onClick = [this] { selectSection (currentSection, true); };
    addAndMakeVisible (playButton);

    stopButton.setColour (TextButton::buttonColourId, Colour (0xffc23d36));
    stopButton.onClick = [this] { proc.stopGroove(); };
    addAndMakeVisible (stopButton);

    loopButton.setClickingTogglesState (true);
    loopButton.setToggleState (proc.grooveLoop.load(), dontSendNotification);
    loopButton.setColour (TextButton::buttonOnColourId, theme::goldDark);
    loopButton.onClick = [this] { proc.grooveLoop.store (loopButton.getToggleState()); };
    addAndMakeVisible (loopButton);

    syncButton.setClickingTogglesState (true);
    syncButton.setToggleState (proc.grooveSyncHost.load(), dontSendNotification);
    syncButton.setColour (TextButton::buttonOnColourId, theme::goldDark);
    syncButton.setTooltip ("Follow the tempo of the DAW (otherwise the groove plays at its own BPM)");
    syncButton.onClick = [this] { proc.grooveSyncHost.store (syncButton.getToggleState()); };
    addAndMakeVisible (syncButton);

    exportButton.setTooltip ("Write the whole MIDI library to a folder on your computer");
    exportButton.onClick = [this] { exportAll(); };
    addAndMakeVisible (exportButton);

    addAndMakeVisible (dragHandle);

    if (! lib.empty())
    {
        int start = 0;
        for (size_t i = 0; i < lib.size(); ++i)
            if (lib[i].name == "Gospel")
                start = (int) i;

        genreBox.setSelectedId (start + 1, sendNotification);
    }
}

void GroovePanel::fillGrooves()
{
    grooveBox.clear (dontSendNotification);

    const int gi = genreBox.getSelectedId() - 1;
    if (! isPositiveAndBelow (gi, (int) lib.size()))
        return;

    int id = 1;
    for (const auto& g : lib[(size_t) gi].grooves)
        grooveBox.addItem (g.name, id++);

    grooveBox.setSelectedId (1, sendNotification);
}

void GroovePanel::buildSectionButtons()
{
    sectionButtons.clear();
    currentSection = 0;

    const int gi = genreBox.getSelectedId() - 1;
    const int ri = grooveBox.getSelectedId() - 1;

    if (! isPositiveAndBelow (gi, (int) lib.size()) || ! isPositiveAndBelow (ri, (int) lib[(size_t) gi].grooves.size()))
        return;

    const auto& groove = lib[(size_t) gi].grooves[(size_t) ri];

    for (size_t i = 0; i < groove.sections.size(); ++i)
    {
        auto b = std::make_unique<TextButton> (groove.sections[i].label);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4711);
        b->setColour (TextButton::buttonOnColourId, theme::gold);
        b->setColour (TextButton::textColourOnId, Colours::black);
        b->onClick = [this, i] { selectSection ((int) i, true); };
        addAndMakeVisible (*b);
        sectionButtons.push_back (std::move (b));
    }

    if (! sectionButtons.empty())
        sectionButtons.front()->setToggleState (true, dontSendNotification);

    currentResource = groove.sections.empty() ? String() : groove.sections.front().resource;
    dragHandle.setResource (currentResource);
    resized();
    repaint();
}

void GroovePanel::selectSection (int index, bool play)
{
    const int gi = genreBox.getSelectedId() - 1;
    const int ri = grooveBox.getSelectedId() - 1;

    if (! isPositiveAndBelow (gi, (int) lib.size()) || ! isPositiveAndBelow (ri, (int) lib[(size_t) gi].grooves.size()))
        return;

    const auto& groove = lib[(size_t) gi].grooves[(size_t) ri];
    if (! isPositiveAndBelow (index, (int) groove.sections.size()))
        return;

    currentSection = index;

    if (index < (int) sectionButtons.size())
        sectionButtons[(size_t) index]->setToggleState (true, dontSendNotification);

    currentResource = groove.sections[(size_t) index].resource;
    dragHandle.setResource (currentResource);

    if (play)
        proc.startGroove (currentResource, true);

    repaint (rollArea);
}

void GroovePanel::exportAll()
{
    const auto def = File::getSpecialLocation (File::userDocumentsDirectory)
                         .getChildFile ("African Gospel").getChildFile ("African Drum").getChildFile ("MIDI");
    def.createDirectory();

    chooser = std::make_unique<FileChooser> ("Choose where to export the MIDI library", def);

    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                          [this] (const FileChooser& fc)
    {
        const auto dir = fc.getResult();
        if (! dir.isDirectory())
            return;

        const int n = proc.exportGrooveLibrary (dir);
        NativeMessageBox::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "African Drum",
                                               String (n) + " MIDI files exported to:\n" + dir.getFullPathName());
    });
}

void GroovePanel::refresh()
{
    const bool playing = proc.isGroovePlaying();
    playButton.setToggleState (playing, dontSendNotification);
    repaint (rollArea.getUnion (bpmArea));
}

void GroovePanel::paint (Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    drawPanel (g, bounds, 10.0f);

    drawPanelTitle (g, Rectangle<int> (0, 4, getWidth(), 22), "GROOVE PLAYER");

    label (g, "GENRE", genreLabel, 10.0f, theme::gold, Justification::centredLeft);
    label (g, "GROOVE", grooveLabel, 10.0f, theme::gold, Justification::centredLeft);

    // BPM readout
    g.setGradientFill (ColourGradient (Colour (0xff130f0c), 0.0f, (float) bpmArea.getY(), Colour (0xff2c221c), 0.0f, (float) bpmArea.getBottom(), false));
    g.fillRoundedRectangle (bpmArea.toFloat(), 4.0f);
    g.setColour (theme::gold.withAlpha (0.5f));
    g.drawRoundedRectangle (bpmArea.toFloat(), 4.0f, 1.0f);
    g.setColour (theme::goldLight);
    g.setFont (Font (FontOptions (15.0f, Font::bold)));
    g.drawText (String (roundToInt (proc.getGrooveBpm())) + " BPM", bpmArea, Justification::centred, false);

    drawGrooveRoll (g, proc, rollArea, currentResource);
}

void GroovePanel::resized()
{
    const bool compact = getWidth() < 900;
    auto r = getLocalBounds().reduced (compact ? 12 : 14, 6);

    titleArea = Rectangle<int> (getWidth() / 2 - 100, 1, 200, 18);
    r.removeFromTop (compact ? 24 : 20);

    if (compact)
    {
        genreLabel = grooveLabel = {};
        auto row1 = r.removeFromTop (28);
        genreBox.setBounds (row1.removeFromLeft (130));
        row1.removeFromLeft (8);
        bpmArea = row1.removeFromRight (84);
        row1.removeFromRight (8);
        grooveBox.setBounds (row1);

        r.removeFromTop (5);
        auto row2 = r.removeFromTop (28);
        playButton.setBounds (row2.removeFromLeft (64));
        row2.removeFromLeft (5);
        stopButton.setBounds (row2.removeFromLeft (64));
        row2.removeFromLeft (5);
        loopButton.setBounds (row2.removeFromLeft (52));
        row2.removeFromLeft (5);
        syncButton.setBounds (row2.removeFromLeft (80));
        exportButton.setBounds (row2.removeFromRight (86));
        row2.removeFromRight (5);
        dragHandle.setBounds (row2);
    }
    else
    {
        auto top = r.removeFromTop (28);
        genreLabel = top.removeFromLeft (46);
        genreBox.setBounds (top.removeFromLeft (130));
        top.removeFromLeft (10);
        grooveLabel = top.removeFromLeft (54);
        grooveBox.setBounds (top.removeFromLeft (190));
        top.removeFromLeft (10);
        bpmArea = top.removeFromLeft (80);
        top.removeFromLeft (14);
        playButton.setBounds (top.removeFromLeft (68));
        top.removeFromLeft (6);
        stopButton.setBounds (top.removeFromLeft (68));
        top.removeFromLeft (6);
        loopButton.setBounds (top.removeFromLeft (56));
        top.removeFromLeft (6);
        syncButton.setBounds (top.removeFromLeft (84));

        exportButton.setBounds (top.removeFromRight (104));
        top.removeFromRight (8);
        dragHandle.setBounds (top.removeFromRight (160));
    }

    r.removeFromTop (compact ? 5 : 8);

    if (! sectionButtons.empty())
    {
        const int n = (int) sectionButtons.size();
        const int rows = compact ? 2 : 1;
        const int perRow = (n + rows - 1) / rows;
        const int rowH = compact ? 24 : 28;

        for (int rr = 0; rr < rows; ++rr)
        {
            auto row = r.removeFromTop (rowH);
            const int w = (row.getWidth() - (perRow - 1) * 4) / perRow;
            for (int k = 0; k < perRow; ++k)
            {
                const int idx = rr * perRow + k;
                if (idx >= n) break;
                sectionButtons[(size_t) idx]->setBounds (row.removeFromLeft (w));
                row.removeFromLeft (4);
            }
            if (rr + 1 < rows) r.removeFromTop (3);
        }
    }

    r.removeFromTop (compact ? 6 : 8);
    rollArea = r;
}

//==============================================================================
DrumKitView::DrumKitView (DrumForgeProcessor& p) : proc (p)
{
    //                     type          name         cx     cy    rx    ry   bodyH tilt    pad shift alt
    parts.push_back ({ Type::Cymbal, "CRASH 1", 199.9f, 81.6f, 39.0f, 10.0f, 0.0f, 0.0f, 12, -1, -1 });
    parts.push_back ({ Type::HiHat, "HI-HAT", 141.8f, 160.5f, 35.0f, 6.0f, 0.0f, 0.0f, 9, 11, -1 });
    parts.push_back ({ Type::Cymbal, "SPLASH", 333.3f, 85.4f, 46.4f, 6.0f, 0.0f, 0.0f, 13, -1, -1 });
    parts.push_back ({ Type::Cymbal, "RIDE", 491.4f, 122.8f, 47.0f, 8.6f, 0.0f, 0.0f, 14, -1, -1 });
    parts.push_back ({ Type::Cymbal, "CRASH 2", 258.6f, 52.7f, 40.4f, 6.0f, 0.0f, 0.0f, 16, -1, -1 });
    parts.push_back ({ Type::Cymbal, "CRASH 3", 408.4f, 57.1f, 40.8f, 6.0f, 0.0f, 0.0f, 17, -1, -1 });
    parts.push_back ({ Type::Cymbal, "SPLASH 2", 105.2f, 124.4f, 30.3f, 7.3f, 0.0f, 0.0f, 18, -1, -1 });
    parts.push_back ({ Type::Cymbal, "CHINA", 566.9f, 119.5f, 41.3f, 11.9f, 0.0f, 0.0f, 19, -1, -1 });
    parts.push_back ({ Type::Cymbal, "FX CYMBAL", 606.5f, 175.4f, 41.1f, 11.6f, 0.0f, 0.0f, 20, -1, -1 });
    parts.push_back ({ Type::Kick, "KICK", 303.0f, 280.9f, 54.0f, 54.0f, 0.0f, 0.0f, 0, -1, 1 });
    parts.push_back ({ Type::Tom, "HIGH TOM", 270.7f, 151.5f, 28.0f, 9.2f, 34.9f, 0.0f, 8, -1, -1 });
    parts.push_back ({ Type::Tom, "MID TOM", 334.9f, 148.0f, 30.4f, 9.9f, 40.0f, 0.0f, 7, -1, -1 });
    parts.push_back ({ Type::Tom, "LOW TOM", 396.3f, 156.1f, 32.6f, 9.7f, 51.8f, 0.0f, 6, -1, -1 });
    parts.push_back ({ Type::Tom, "FLOOR TOM", 442.2f, 216.8f, 39.4f, 6.5f, 76.6f, 0.0f, 5, -1, -1 });
    parts.push_back ({ Type::Snare, "SNARE", 209.2f, 203.6f, 34.4f, 6.8f, 26.5f, 0.0f, 2, 4, -1 });
    parts.push_back ({ Type::Pedal, "HH PEDAL", 137.2f, 327.7f, 18.0f, 9.0f, 0.0f, 0.0f, 10, -1, -1 });
}

int DrumKitView::partForPad (int pad) const
{
    for (size_t i = 0; i < parts.size(); ++i)
        if (parts[i].pad == pad || parts[i].padShift == pad || parts[i].padAlt == pad)
            return (int) i;

    if (pad == 3)  return 9;    // snare rim
    if (pad == 15) return 3;    // ride bell
    return -1;
}

void DrumKitView::setSelectedPad (int pad)
{
    selectedPad = pad;
    repaint();
}

Point<float> DrumKitView::toDesign (Point<float> p) const
{
    return p.transformedBy (xf.inverted());
}

int DrumKitView::hitTest (Point<float> p, float* normDist) const
{
    static const int order[] = { 10, 9, 8, 7, 6, 5, 4, 1, 3, 2, 0 };

    for (int idx : order)
    {
        const auto& pt = parts[(size_t) idx];
        const float dx = (p.x - pt.cx) / pt.rx;

        float dy = (p.y - pt.cy) / pt.ry;
        bool inside = false;

        switch (pt.type)
        {
            case Type::Kick:
                inside = std::hypot (p.x - pt.cx, p.y - pt.cy) <= pt.rx;
                dy = (p.y - pt.cy) / pt.ry;
                break;

            case Type::Tom:
            case Type::Snare:
                inside = (dx * dx + dy * dy <= 1.0f)
                      || (std::abs (p.x - pt.cx) <= pt.rx && p.y >= pt.cy && p.y <= pt.cy + pt.bodyH);
                break;

            case Type::Cymbal:
            case Type::HiHat:
            case Type::Pedal:
                dy = (p.y - pt.cy) / (pt.ry * 1.7f);
                inside = dx * dx + dy * dy <= 1.0f;
                break;
        }

        if (inside)
        {
            if (normDist != nullptr)
                *normDist = jlimit (0.0f, 1.0f, std::sqrt (dx * dx + ((p.y - pt.cy) / pt.ry) * ((p.y - pt.cy) / pt.ry)));
            return idx;
        }
    }

    return -1;
}

void DrumKitView::mouseDown (const MouseEvent& e)
{
    float nd = 1.0f;
    const int idx = hitTest (toDesign (e.position), &nd);
    if (idx < 0)
        return;

    const auto& pt = parts[(size_t) idx];
    int pad = pt.pad;

    switch (pt.type)
    {
        case Type::Kick:   if (e.mods.isAltDown()) pad = pt.padAlt; break;
        case Type::Snare:  if (e.mods.isShiftDown()) pad = pt.padShift; else if (nd > 0.82f) pad = 3; break;
        case Type::HiHat:  if (e.mods.isShiftDown()) pad = pt.padShift; break;
        case Type::Cymbal: if (pt.pad == 14 && nd < 0.22f) pad = 15; break;
        default: break;
    }

    if (! e.mods.isPopupMenu())      // right click only opens the sound chooser
        proc.triggerPad (pad, jlimit (0.5f, 1.0f, 1.0f - 0.5f * nd));
    setSelectedPad (pad);

    if (onPadSelected)
        onPadSelected (pad);

    // Right click on a kit piece (kick, snare, toms...): offer the same piece from each of the 3 kits
    if (pad < 9 && e.mods.isPopupMenu())
    {
        const auto& list = swapList();
        const auto cur = proc.peekSample (pad);
        const String curId = cur != nullptr ? cur->factoryId : String();

        PopupMenu m;
        String title;
        for (size_t i = 0; i < list.size(); ++i)
            if (list[i].pad == pad && list[i].kitIndex < 3)
            {
                if (title.isEmpty())
                    title = list[i].name;
                m.addItem (1 + (int) i, list[i].kit + "   -   " + list[i].name, true, list[i].resource == curId);
            }

        if (m.getNumItems() > 0)
        {
            PopupMenu real;
            real.addSectionHeader ("Choose the sound:  " + title);
            real.addSeparator();
            for (size_t i = 0; i < list.size(); ++i)
                if (list[i].pad == pad && list[i].kitIndex < 3)
                    real.addItem (1 + (int) i, list[i].kit + "   -   " + list[i].name, true, list[i].resource == curId);

            Component::SafePointer<DrumKitView> safe (this);
            real.showMenuAsync (PopupMenu::Options().withTargetScreenArea (Rectangle<int> (e.getScreenX(), e.getScreenY(), 1, 1)).withMinimumWidth (230),
                                [safe, pad] (int r)
                                {
                                    if (safe == nullptr || r <= 0)
                                        return;

                                    if (safe->proc.loadFactorySample (pad, swapList()[(size_t) (r - 1)].resource))
                                    {
                                        safe->proc.triggerPad (pad, 0.9f);
                                        if (safe->onPadSelected)
                                            safe->onPadSelected (pad);
                                    }
                                });
        }
    }
}

void DrumKitView::mouseMove (const MouseEvent& e)
{
    const int idx = hitTest (toDesign (e.position));
    setMouseCursor (idx >= 0 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);

    if (idx != hoverPart)
    {
        hoverPart = idx;
        repaint();
    }
}

bool DrumKitView::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (proc.isSupportedFile (File (f)))
            return true;

    return false;
}

void DrumKitView::filesDropped (const StringArray& files, int x, int y)
{
    const int idx = hitTest (toDesign (Point<float> ((float) x, (float) y)));
    int target = idx >= 0 ? parts[(size_t) idx].pad : selectedPad;

    for (auto& path : files)
    {
        if (target >= df::numPads)
            break;

        const File f (path);
        if (proc.isSupportedFile (f) && proc.loadSample (target, f))
            ++target;
    }

    repaint();
}

void DrumKitView::refresh()
{
    bool changed = false;

    for (auto& pt : parts)
    {
        float lv = proc.padLevel[(size_t) pt.pad].load();

        if (pt.padShift >= 0) lv = jmax (lv, proc.padLevel[(size_t) pt.padShift].load());
        if (pt.padAlt >= 0)   lv = jmax (lv, proc.padLevel[(size_t) pt.padAlt].load());
        if (pt.pad == 2)      lv = jmax (lv, proc.padLevel[3].load());
        if (pt.pad == 14)     lv = jmax (lv, proc.padLevel[15].load());

        const float g = jmax (jlimit (0.0f, 1.0f, lv * 2.4f), pt.glow * 0.84f);
        if (std::abs (g - pt.glow) > 0.01f)
            changed = true;

        pt.glow = g < 0.01f ? 0.0f : g;
    }

    if (changed)
        repaint();
}

void DrumKitView::drawPole (Graphics& g, float x1, float y1, float x2, float y2, float w) const
{
    g.setColour (Colour (0xff271e19));
    g.drawLine (x1, y1, x2, y2, w + 1.6f);
    g.setColour (Colour (0xffffcda7));
    g.drawLine (x1, y1, x2, y2, w * 0.6f);
}

void DrumKitView::drawTripod (Graphics& g, float x, float y, float w) const
{
    drawPole (g, x, y, x - w, y + 12.0f, 3.0f);
    drawPole (g, x, y, x + w, y + 12.0f, 3.0f);
    drawPole (g, x, y, x + w * 0.2f, y + 16.0f, 3.0f);

    g.setColour (Colour (0xff1b1511));
    for (float dx : { -w, w, w * 0.2f })
        g.fillEllipse (x + dx - 3.5f, y + (dx == w * 0.2f ? 15.0f : 11.0f), 7.0f, 4.0f);
}

void DrumKitView::drawCymbal (Graphics& g, const Part& pt, bool hat) const
{
    Graphics::ScopedSaveState ss (g);
    g.addTransform (AffineTransform::rotation (pt.tilt, pt.cx, pt.cy));

    auto draw = [&] (float cy, float darken)
    {
        const Rectangle<float> r (pt.cx - pt.rx, cy - pt.ry, pt.rx * 2.0f, pt.ry * 2.0f);

        ColourGradient grad (Colour (0xff7d4713).darker (darken), r.getX(), 0.0f,
                             Colour (0xff7d4713).darker (darken), r.getRight(), 0.0f, false);
        grad.addColour (0.28, Colour (0xffffd9a3).darker (darken));
        grad.addColour (0.5, Colour (0xffe8923a).darker (darken));
        grad.addColour (0.72, Colour (0xffffbf6e).darker (darken));
        g.setGradientFill (grad);
        g.fillEllipse (r);

        g.setColour (Colours::black.withAlpha (0.14f));
        for (int k = 1; k <= 5; ++k)
        {
            const float f = (float) k / 6.0f;
            g.drawEllipse (pt.cx - pt.rx * f, cy - pt.ry * f, pt.rx * f * 2.0f, pt.ry * f * 2.0f, 0.8f);
        }

        g.setColour (Colour (0xff5a330f));
        g.drawEllipse (r, 1.3f);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawEllipse (r.reduced (1.5f), 0.8f);

        // bell
        const Rectangle<float> bell (pt.cx - pt.rx * 0.2f, cy - pt.ry * 0.62f, pt.rx * 0.4f, pt.ry * 1.0f);
        g.setGradientFill (ColourGradient (Colour (0xffffe3bf).darker (darken), bell.getCentreX(), bell.getY(),
                                           Colour (0xffa85f1d).darker (darken), bell.getCentreX(), bell.getBottom(), false));
        g.fillEllipse (bell);
    };

    if (hat)
        draw (pt.cy + 7.0f, 0.5f);

    draw (pt.cy, 0.0f);

    if (pt.glow > 0.02f)
    {
        ColourGradient halo (theme::goldLight.withAlpha (pt.glow * 0.75f), pt.cx, pt.cy, Colours::transparentBlack, pt.cx + pt.rx * 1.25f, pt.cy, true);
        g.setGradientFill (halo);
        g.fillEllipse (pt.cx - pt.rx * 1.3f, pt.cy - pt.ry * 3.0f, pt.rx * 2.6f, pt.ry * 6.0f);
    }
}

void DrumKitView::drawDrum (Graphics& g, const Part& pt) const
{
    const float cx = pt.cx, cy = pt.cy, rx = pt.rx, ry = pt.ry, bh = pt.bodyH;

    // shadow on the floor
    g.setColour (Colours::black.withAlpha (0.28f));
    g.fillEllipse (cx - rx * 1.05f, cy + bh + ry * 0.4f, rx * 2.1f, ry * 1.2f);

    ColourGradient shell (Colour (0xff755b4a), cx - rx, 0.0f, Colour (0xff2d231c), cx + rx, 0.0f, false);
    shell.addColour (0.22, Colour (0xffffe5bb));
    shell.addColour (0.42, Colour (0xffc89b7f));
    shell.addColour (0.75, Colour (0xff675041));

    g.setGradientFill (shell);
    g.fillEllipse (cx - rx, cy + bh - ry, rx * 2.0f, ry * 2.0f);
    g.fillRect (cx - rx, cy, rx * 2.0f, bh);

    // bottom hoop line
    g.setColour (Colour (0xffffd6af).withAlpha (0.8f));
    g.drawEllipse (cx - rx, cy + bh - ry, rx * 2.0f, ry * 2.0f, 1.6f);
    g.setColour (shell.getColour (0).withAlpha (0.0f));

    // lugs
    for (int i = 0; i < 5; ++i)
    {
        const float x = cx - rx * 0.78f + (float) i * rx * 0.39f;
        g.setGradientFill (ColourGradient (Colour (0xffffd9a3), x - 2.0f, 0.0f, Colour (0xff9c5719), x + 2.0f, 0.0f, false));
        g.fillRoundedRectangle (x - 2.0f, cy + bh * 0.28f, 4.0f, bh * 0.44f, 1.2f);
    }

    // hoop + head
    const Rectangle<float> top (cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
    g.setColour (Colour (0xff261d18));
    g.fillEllipse (top);
    g.setGradientFill (ColourGradient (Colour (0xfffbfdff), cx, cy - ry, Colour (0xffca9d80), cx, cy + ry, false));
    g.drawEllipse (top, 3.2f);

    const auto head = top.reduced (rx * 0.09f, ry * 0.09f);
    g.setGradientFill (ColourGradient (Colour (0xffffffff), cx - rx * 0.2f, cy - ry * 0.3f, Colour (0xffcfcab9), cx + rx, cy + ry, true));
    g.fillEllipse (head);

    g.setColour (Colours::white.withAlpha (0.55f));
    g.drawEllipse (head.reduced (rx * 0.25f, ry * 0.25f), 0.8f);

    if (pt.type == Type::Snare)
    {
        g.setColour (Colours::black.withAlpha (0.10f));
        for (int i = -3; i <= 3; ++i)
            g.drawLine (cx - rx * 0.8f, cy + (float) i * ry * 0.18f, cx + rx * 0.8f, cy + (float) i * ry * 0.18f, 0.8f);
    }

    if (pt.glow > 0.02f)
    {
        g.setGradientFill (ColourGradient (theme::gold.withAlpha (pt.glow * 0.9f), cx, cy, Colours::transparentBlack, cx + rx * 1.2f, cy, true));
        g.fillEllipse (top.expanded (rx * 0.25f, ry * 0.6f));
    }
}

void DrumKitView::drawKick (Graphics& g, const Part& pt) const
{
    const float cx = pt.cx, cy = pt.cy, r = pt.rx;

    g.setColour (Colours::black.withAlpha (0.32f));
    g.fillEllipse (cx - r * 1.05f, cy + r * 0.78f, r * 2.1f, r * 0.5f);

    // spurs
    drawPole (g, cx - r * 0.82f, cy + r * 0.62f, cx - r * 1.12f, cy + r * 1.0f, 4.0f);
    drawPole (g, cx + r * 0.82f, cy + r * 0.62f, cx + r * 1.12f, cy + r * 1.0f, 4.0f);

    // shell + gold hoop
    g.setGradientFill (ColourGradient (Colour (0xff94735e), cx, cy - r, Colour (0xff1b1511), cx, cy + r, false));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);

    ColourGradient hoop (Colour (0xffffd9a3), cx - r, cy - r, Colour (0xff9c5719), cx + r, cy + r, false);
    hoop.addColour (0.5, Colour (0xffe8923a));
    g.setGradientFill (hoop);
    g.drawEllipse (cx - r + 3.0f, cy - r + 3.0f, (r - 3.0f) * 2.0f, (r - 3.0f) * 2.0f, 7.0f);
    g.setColour (Colour (0xffffe1b8));
    g.drawEllipse (cx - r + 11.0f, cy - r + 11.0f, (r - 11.0f) * 2.0f, (r - 11.0f) * 2.0f, 2.0f);

    // head
    const float hr = r - 13.0f;
    g.setGradientFill (ColourGradient (Colour (0xffffffff), cx - hr * 0.3f, cy - hr * 0.4f, Colour (0xffc2bdac), cx + hr, cy + hr, true));
    g.fillEllipse (cx - hr, cy - hr, hr * 2.0f, hr * 2.0f);

    // port hole
    g.setColour (Colour (0xff0b0e12));
    g.fillEllipse (cx + hr * 0.38f, cy + hr * 0.42f, hr * 0.34f, hr * 0.34f);
    g.setColour (theme::goldDark);
    g.drawEllipse (cx + hr * 0.38f, cy + hr * 0.42f, hr * 0.34f, hr * 0.34f, 1.6f);

    // monogram
    g.setColour (Colour (0xffa85f1d));
    g.setFont (Font (FontOptions (hr * 0.62f, Font::bold)));
    g.drawText ("AG", Rectangle<float> (cx - hr, cy - hr * 0.62f, hr * 2.0f, hr * 0.8f), Justification::centred, false);
    g.setColour (Colour (0xff8f6e5a));
    g.setFont (Font (FontOptions (hr * 0.16f, Font::bold)));
    g.drawText ("AFRICAN GOSPEL", Rectangle<float> (cx - hr, cy + hr * 0.08f, hr * 2.0f, hr * 0.3f), Justification::centred, false);

    if (pt.glow > 0.02f)
    {
        g.setGradientFill (ColourGradient (theme::gold.withAlpha (pt.glow * 0.8f), cx, cy, Colours::transparentBlack, cx + r * 1.1f, cy, true));
        g.fillEllipse (cx - r * 1.1f, cy - r * 1.1f, r * 2.2f, r * 2.2f);
    }
}

void DrumKitView::paint (Graphics& g)
{
    const float W = 660.0f, H = 404.0f;
    const float usableH = (float) getHeight() - 18.0f;
    const float s = jmin ((float) getWidth() / W, usableH / H);
    const float ox = ((float) getWidth() - W * s) * 0.5f, oy = (usableH - H * s) * 0.5f;
    xf = AffineTransform::scale (s).translated (ox, oy);

    // photo-real studio render of the kit (embedded), drawn in design space
    static const juce::Image scene = juce::ImageCache::getFromMemory (BinaryData::kit_scene_png, BinaryData::kit_scene_pngSize);

    g.fillAll (Colour (0xff140f0c));

    // extend the stage to the side bars with the edge columns of the render (seamless, no ghosting)
    if (scene.isValid())
    {
        Graphics::ScopedSaveState bg (g);
        g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
        const int iw = scene.getWidth(), ih = scene.getHeight();
        const int mainW = roundToInt (W * s), mainH = roundToInt (H * s);
        const int mx = roundToInt (ox), my = roundToInt (oy);
        if (mx > 0)
        {
            g.drawImage (scene, 0, my, mx + 1, mainH, 0, 0, 4, ih);
            g.drawImage (scene, mx + mainW - 1, my, getWidth() - (mx + mainW) + 1, mainH, iw - 4, 0, 4, ih);
        }
    }

    g.setColour (Colours::white.withAlpha (0.60f));
    g.setFont (Font (FontOptions (10.5f)));
    g.drawText ("Click a drum to play   |   Shift: open hat / side stick   |   Alt: kick 2   |   Click ride bell or snare rim",
                Rectangle<float> (0.0f, (float) getHeight() - 17.0f, (float) getWidth(), 14.0f), Justification::centred, false);

    Graphics::ScopedSaveState ss (g);
    g.addTransform (xf);

    {
        Path clip;
        clip.addRoundedRectangle (0.0f, 0.0f, W, H, 6.0f);
        g.reduceClipRegion (clip.createPathWithRoundedCorners (0.0f).getBounds().toNearestInt());
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (scene, Rectangle<float> (0.0f, 0.0f, W, H), RectanglePlacement::stretchToFit);
    }

    // hit glow on the part that is sounding
    for (const auto& pt : parts)
    {
        if (pt.glow < 0.02f)
            continue;

        const bool kick = pt.type == Type::Kick;
        const float rx = pt.rx + 8.0f, ry = (kick ? pt.rx : pt.ry) + 8.0f;
        const float cy = pt.cy + (pt.type == Type::Tom || pt.type == Type::Snare ? 0.0f : 0.0f);
        g.setGradientFill (ColourGradient (Colour (0xfff5a748).withAlpha (pt.glow * 0.55f), pt.cx, cy,
                                           Colours::transparentBlack, pt.cx + rx, cy, true));
        g.fillEllipse (pt.cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
    }

    // ---- selection / hover rings
    auto ring = [&] (int idx, Colour c, float thick)
    {
        if (idx < 0)
            return;

        const auto& pt = parts[(size_t) idx];
        g.setColour (c);

        if (pt.type == Type::Kick)
            g.drawEllipse (pt.cx - pt.rx - 5.0f, pt.cy - pt.rx - 5.0f, (pt.rx + 5.0f) * 2.0f, (pt.rx + 5.0f) * 2.0f, thick);
        else
            g.drawEllipse (pt.cx - pt.rx - 6.0f, pt.cy - pt.ry - 6.0f, (pt.rx + 6.0f) * 2.0f, (pt.ry + 6.0f) * 2.0f, thick);
    };

    ring (partForPad (selectedPad), theme::gold.withAlpha (0.9f), 2.2f);
    if (hoverPart != partForPad (selectedPad))
        ring (hoverPart, Colours::white.withAlpha (0.55f), 1.6f);

    // ---- captions (in design space)
    const int sel = partForPad (selectedPad);
    String caption = sel >= 0 ? String (parts[(size_t) sel].name) : String();

    if (selectedPad != (sel >= 0 ? parts[(size_t) sel].pad : -1))
        caption = DrumForgeProcessor::getPadNames()[selectedPad].toUpperCase();

    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (8.0f, 8.0f, 214.0f, 24.0f, 5.0f);
    g.setColour (theme::goldLight);
    g.setFont (Font (FontOptions (12.0f, Font::bold)));
    g.drawText ("SELECTED  " + caption + "  -  " + MidiMessage::getMidiNoteName (df::firstNote + selectedPad, true, true, 3),
                Rectangle<float> (16.0f, 8.0f, 206.0f, 24.0f), Justification::centredLeft, false);

}

//==============================================================================
class KitSelectorPanel::Grid : public Component
{
public:
    explicit Grid (DrumForgeProcessor& p) : proc (p) {}

    struct Entry { String name; int index; Colour tint; };

    void setFilter (const String& f)
    {
        entries.clear();
        const auto kits = proc.getKits();

        for (size_t i = 0; i < kits.size(); ++i)
            if (f.isEmpty() || kits[i].name.containsIgnoreCase (f))
                entries.push_back ({ kits[i].name, (int) i, kitTint (kits[i]) });

        layout();
        repaint();
    }

    void layout()
    {
        const int rows = (int) entries.size() + 1;
        setSize (jmax (100, getParentWidth()), jmax (10, rows * cardH + 4));
    }

    Rectangle<int> cardBounds (int i) const
    {
        return Rectangle<int> (2, 2 + i * cardH, getWidth() - 4, cardH - 2);
    }

    int rowOfEntryNamed (const String& name) const
    {
        for (size_t i = 0; i < entries.size(); ++i)
            if (entries[i].name == name)
                return (int) i;
        return -1;
    }

    std::function<void()> onAdd;

    void paintAddCard (Graphics& g)
    {
        const auto r = cardBounds ((int) entries.size()).toFloat().reduced (1.0f);
        const bool hover = hoverIndex == (int) entries.size();

        g.setColour (hover ? Colour (0xffe8923a).withAlpha (0.20f) : Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (Colour (0xffe8923a).withAlpha (hover ? 1.0f : 0.6f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (Colour (0xffffd9a3));
        g.setFont (Font (FontOptions (11.0f, Font::bold)));
        g.drawText ("+  ADD YOUR KIT  (open kits folder)", r, Justification::centred, false);
    }

    void paint (Graphics& g) override
    {
        const auto current = proc.getCurrentKitName();
        paintAddCard (g);

        for (int i = 0; i < (int) entries.size(); ++i)
        {
            const auto& e = entries[(size_t) i];
            const auto r = cardBounds (i).toFloat().reduced (1.0f);
            const bool selected = e.name == current;
            const bool hover = i == hoverIndex;

            if (selected)
            {
                g.setGradientFill (ColourGradient (Colour (0xffe8923a), 0.0f, r.getY(), Colour (0xff9c5719), 0.0f, r.getBottom(), false));
                g.fillRoundedRectangle (r, 4.0f);
            }
            else
            {
                g.setColour (hover ? Colour (0xff3a261a) : (i % 2 ? Colour (0xff1a110b) : Colour (0xff22160e)));
                g.fillRoundedRectangle (r, 4.0f);
            }

            g.setColour (selected ? Colour (0xff1a0e06) : Colour (0xffe8923a));
            g.setFont (Font (FontOptions (11.0f, Font::bold)));
            g.drawText (String (i + 1).paddedLeft ('0', 2), r.withTrimmedLeft (8.0f).withWidth (24.0f), Justification::centredLeft, false);

            g.setColour (selected ? Colour (0xff1a0e06) : Colour (0xfff7e1c4));
            g.setFont (Font (FontOptions (12.0f, Font::bold)));
            g.drawText (e.name, r.withTrimmedLeft (34.0f).withTrimmedRight (6.0f), Justification::centredLeft, true);
        }
    }

    void mouseMove (const MouseEvent& e) override
    {
        int h = -1;
        for (int i = 0; i <= (int) entries.size(); ++i)
            if (cardBounds (i).contains (e.getPosition()))
                h = i;

        if (h != hoverIndex)
        {
            hoverIndex = h;
            setMouseCursor (h >= 0 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseExit (const MouseEvent&) override { hoverIndex = -1; repaint(); }

    void mouseDown (const MouseEvent& e) override
    {
        if (cardBounds ((int) entries.size()).contains (e.getPosition()))
        {
            if (onAdd)
                onAdd();
            return;
        }

        for (int i = 0; i < (int) entries.size(); ++i)
            if (cardBounds (i).contains (e.getPosition()))
            {
                proc.loadKit (entries[(size_t) i].index);
                repaint();
                return;
            }
    }

    int cardH = 24;

private:
    DrumForgeProcessor& proc;
    std::vector<Entry> entries;
    int hoverIndex = -1;
};

KitSelectorPanel::KitSelectorPanel (DrumForgeProcessor& p) : proc (p)
{
    grid = std::make_unique<Grid> (proc);

    search.setTextToShowWhenEmpty ("Search kits...", theme::textDim);
    search.setFont (Font (FontOptions (13.0f)));
    search.setColour (TextEditor::backgroundColourId, Colour (0xff1d1612));
    search.setColour (TextEditor::outlineColourId, Colour (0xff120e0b));
    search.setColour (TextEditor::focusedOutlineColourId, theme::gold);
    search.onTextChange = [this] { grid->setFilter (search.getText()); };
    addAndMakeVisible (search);

    viewport.setViewedComponent (grid.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    kitsFolderButton.setTooltip ("Put a folder of samples per kit here (DW, Sonor, Mapex, Tama...). They appear in the list.");
    kitsFolderButton.onClick = [this] { openKitsFolder(); };

    addAndMakeVisible (kitsFolderButton);

    importButton.setTooltip ("Load a folder of samples straight onto the pads");
    importButton.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Choose a folder of drum samples", proc.lastDirectory);

        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                              [this] (const FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (dir.isDirectory())
                proc.loadFolder (dir);
        });
    };
    addAndMakeVisible (importButton);
    if (! df::externalSamplesEnabled)
    {
        importButton.setEnabled (false);
        importButton.setTooltip ("Only samples inside the kits folder can be loaded - use MY KITS");
    }

    grid->onAdd = [this] { openKitsFolder(); };
    knownKits = proc.getKits().size();
    grid->setFilter ({});
}

KitSelectorPanel::~KitSelectorPanel() { viewport.setViewedComponent (nullptr, false); }

void KitSelectorPanel::paint (Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat().reduced (1.0f), 8.0f);
    drawPanelTitle (g, Rectangle<int> (0, 4, getWidth(), 24), "KIT PRESETS");
}

void KitSelectorPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromTop (24);
    search.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);

    auto bottom = r.removeFromBottom (28);
    kitsFolderButton.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2 - 2));
    bottom.removeFromLeft (4);
    importButton.setBounds (bottom);
    r.removeFromBottom (6);

    viewport.setBounds (r);
    grid->setSize (viewport.getMaximumVisibleWidth(), grid->getHeight());
    grid->layout();
}

void KitSelectorPanel::refresh()
{
    if (++rescanCounter >= 90)
    {
        rescanCounter = 0;
        proc.rescanKits();

        if (proc.getKits().size() != knownKits)
        {
            knownKits = proc.getKits().size();
            grid->setFilter (search.getText());
        }
    }

    // keep the loaded kit visible in the list
    const auto current = proc.getCurrentKitName();
    if (current != lastShownKit)
    {
        lastShownKit = current;

        if (const int i = grid->rowOfEntryNamed (current); i >= 0)
        {
            const auto r = grid->cardBounds (i);
            const int top = viewport.getViewPositionY(), h = viewport.getViewHeight();

            if (r.getY() < top)             viewport.setViewPosition (0, r.getY() - 2);
            else if (r.getBottom() > top + h) viewport.setViewPosition (0, r.getBottom() - h + 2);
        }
    }

    grid->repaint();
}

void KitSelectorPanel::stepKit (int delta)
{
    const auto kits = proc.getKits();
    if (kits.empty())
        return;

    const auto cur = proc.getCurrentKitName();
    int idx = -1;
    for (size_t i = 0; i < kits.size(); ++i)
        if (kits[i].name == cur)
            idx = (int) i;

    idx = idx < 0 ? 0 : (idx + delta + (int) kits.size()) % (int) kits.size();
    proc.loadKit (idx);
}

void KitSelectorPanel::openKitsFolder()
{
    const auto folder = DrumForgeProcessor::getKitsFolder();
    folder.createDirectory();

    const auto readme = folder.getChildFile ("READ ME.txt");
    if (! readme.existsAsFile())
        readme.replaceWithText (
            "African Drum - your kits\n\n"
            "Create one sub-folder per kit and put your samples inside, for example:\n"
            "  DW Collectors\\Kick.wav, Snare.wav, Tom Floor.wav, HiHat Closed.wav, Crash 1.wav ...\n"
            "  Sonor SQ2\\...\n  Mapex Saturn\\...\n  Tama Starclassic\\...\n\n"
            "Names are recognised automatically (kick, snare, rim, side stick, tom floor/low/mid/high,\n"
            "hat closed/pedal/open, crash, ride, bell). Without recognisable names the files are\n"
            "assigned alphabetically to pads 1-16. WAV, AIFF, FLAC, OGG and MP3 are supported.\n"
            "The kit appears in the plugin's kit list within a few seconds.\n");

    proc.rescanKits();
    folder.revealToUser();
}

//==============================================================================
RackPanel::RackPanel (DrumForgeProcessor& p) : proc (p)
{
    auto knob = [this] (Slider& s)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::TextBoxBelow, false, 56, 13);
        s.setTextBoxIsEditable (false);
        addAndMakeVisible (s);
    };

    for (auto* s : { &pitchSlider, &decaySlider, &panSlider, &roomSizeSlider, &roomMixSlider, &humanizeSlider })
        knob (*s);

    faderSlider.setSliderStyle (Slider::LinearVertical);
    faderSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    faderSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (faderSlider);

    for (auto* b : { &muteButton, &soloButton, &gateButton })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }

    muteButton.setColour (TextButton::buttonOnColourId, theme::red);
    soloButton.setColour (TextButton::buttonOnColourId, Colour (0xffe8923a));
    soloButton.setColour (TextButton::textColourOnId, Colours::black);
    gateButton.setColour (TextButton::buttonOnColourId, theme::gold);
    gateButton.setColour (TextButton::textColourOnId, Colours::black);

    loadButton.onClick = [this] { showLoadMenu(); };
    loadButton.setTooltip ("Load a file, or swap in any sample from the built-in kits");
    clearButton.onClick = [this] { proc.clearSample (pad); };
    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearButton);

    if (auto* c = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (df::pid ("out", 0))))
        outBox.addItemList (c->choices, 1);
    if (auto* c = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (df::pid ("choke", 0))))
        chokeBox.addItemList (c->choices, 1);

    addAndMakeVisible (outBox);
    addAndMakeVisible (chokeBox);

    roomSizeAtt = std::make_unique<SliderAtt> (proc.apvts, "roomSize", roomSizeSlider);
    roomMixAtt  = std::make_unique<SliderAtt> (proc.apvts, "roomMix", roomMixSlider);
    humanizeAtt = std::make_unique<SliderAtt> (proc.apvts, "humanize", humanizeSlider);

    for (auto* s : { &roomSizeSlider, &roomMixSlider, &humanizeSlider })
    {
        s->textFromValueFunction = [] (double v) { return String (roundToInt (v * 100.0)) + "%"; };
        s->updateText();
    }

    bind (2);
}

void RackPanel::bind (int newPad)
{
    pad = jlimit (0, df::numPads - 1, newPad);

    pitchAtt.reset(); decayAtt.reset(); panAtt.reset(); faderAtt.reset();
    muteAtt.reset(); soloAtt.reset(); gateAtt.reset(); outAtt.reset(); chokeAtt.reset();

    auto& a = proc.apvts;
    pitchAtt = std::make_unique<SliderAtt> (a, df::pid ("pitch", pad), pitchSlider);
    decayAtt = std::make_unique<SliderAtt> (a, df::pid ("decay", pad), decaySlider);
    panAtt   = std::make_unique<SliderAtt> (a, df::pid ("pan", pad), panSlider);
    faderAtt = std::make_unique<SliderAtt> (a, df::pid ("gain", pad), faderSlider);
    muteAtt  = std::make_unique<ButtonAtt> (a, df::pid ("mute", pad), muteButton);
    soloAtt  = std::make_unique<ButtonAtt> (a, df::pid ("solo", pad), soloButton);
    gateAtt  = std::make_unique<ButtonAtt> (a, df::pid ("gate", pad), gateButton);
    outAtt   = std::make_unique<ComboAtt>  (a, df::pid ("out", pad), outBox);
    chokeAtt = std::make_unique<ComboAtt>  (a, df::pid ("choke", pad), chokeBox);

    pitchSlider.setDoubleClickReturnValue (true, 0.0);
    decaySlider.setDoubleClickReturnValue (true, 5.0);
    panSlider.setDoubleClickReturnValue (true, 0.0);
    panSlider.textFromValueFunction = [] (double v)
    {
        if (std::abs (v) < 0.005)
            return String ("C");
        return String (roundToInt (std::abs (v) * 100.0)) + (v < 0 ? " L" : " R");
    };

    pitchSlider.updateText();
    decaySlider.updateText();
    panSlider.updateText();
    repaint();
}

void RackPanel::showLoadMenu()
{
    PopupMenu m;
    m.addItem (1, "Load sample from file...");
    addSwapMenu (m, 1000);

    Component::SafePointer<RackPanel> safe (this);

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&loadButton), [safe] (int result)
    {
        if (safe == nullptr)
            return;

        if (result == 1)
            safe->chooseFile();
        else if (result >= 1000)
            safe->proc.loadFactorySample (safe->pad, swapList()[(size_t) (result - 1000)].resource);
    });
}

void RackPanel::chooseFile()
{
    chooser = std::make_unique<FileChooser> ("Choose a sample for " + DrumForgeProcessor::getPadNames()[pad],
                                             proc.lastDirectory, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

    Component::SafePointer<RackPanel> safe (this);

    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe] (const FileChooser& fc)
    {
        if (safe == nullptr)
            return;

        const auto f = fc.getResult();
        if (f.existsAsFile())
            safe->proc.loadSample (safe->pad, f);
    });
}

void RackPanel::refresh()
{
    const float lv = proc.padLevel[(size_t) pad].load();
    const float m = jmax (lv, meter * 0.88f);

    if (std::abs (m - meter) > 0.0005f)
    {
        meter = m;
        repaint (meterArea);
    }

    repaint (nameArea);
}

void RackPanel::paint (Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat().reduced (1.0f), 10.0f);

    auto head = Rectangle<float> (1.0f, 1.0f, (float) getWidth() - 2.0f, 28.0f);
    Path hp;
    hp.addRoundedRectangle (head.getX(), head.getY(), head.getWidth(), head.getHeight(), 10.0f, 10.0f, true, true, false, false);
    g.setGradientFill (ColourGradient (Colour (0xff7f6250), 0.0f, head.getY(), Colour (0xff44352b), 0.0f, head.getBottom(), false));
    g.fillPath (hp);
    label (g, "MIXER & FX RACK", head.toNearestInt(), 12.0f, Colours::white);

    // selected pad title
    const auto accent = accentForPad (pad);
    g.setColour (accent);
    g.fillRoundedRectangle ((float) nameArea.getX(), (float) nameArea.getY() + 3.0f, 4.0f, (float) nameArea.getHeight() - 6.0f, 2.0f);

    auto t = nameArea.withTrimmedLeft (12).withTrimmedRight (96);
    label (g, DrumForgeProcessor::getPadNames()[pad].toUpperCase() + "  -  "
                 + MidiMessage::getMidiNoteName (df::firstNote + pad, true, true, 3),
           t.removeFromTop (20), 14.0f, Colours::white, Justification::centredLeft);

    const auto sn = proc.getSampleName (pad);
    label (g, sn.isEmpty() ? String ("- empty -") : sn, t, 11.0f, sn.isEmpty() ? theme::textDim : theme::goldLight,
           Justification::centredLeft);

    label (g, "TUNING", knobLabelArea[0], 9.5f);
    label (g, "DECAY",  knobLabelArea[1], 9.5f);
    label (g, "PAN",    knobLabelArea[2], 9.5f);

    drawSegmentMeter (g, meterArea, meter, accent);

    // FX strip
    g.setColour (Colour (0xff15110d));
    g.fillRoundedRectangle (fxArea.toFloat(), 6.0f);
    g.setColour (theme::gold.withAlpha (0.35f));
    g.drawRoundedRectangle (fxArea.toFloat(), 6.0f, 1.0f);

    label (g, "ROOM",     fxLabelArea[0], 9.5f, theme::gold);
    label (g, "MIX",      fxLabelArea[1], 9.5f, theme::gold);
    label (g, "HUMANIZE", fxLabelArea[2], 9.5f, theme::gold);
}

void RackPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    r.removeFromTop (28);

    nameArea = r.removeFromTop (42);
    auto btns = nameArea.removeFromRight (90).withSizeKeepingCentre (90, 22);
    loadButton.setBounds (btns.removeFromLeft (43));
    btns.removeFromLeft (4);
    clearButton.setBounds (btns);
    nameArea = Rectangle<int> (r.getX(), nameArea.getY(), r.getWidth(), 42);
    r.removeFromTop (4);

    auto knobsRow = r.removeFromTop (84);
    const int kw = knobsRow.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto col = knobsRow.removeFromLeft (kw);
        knobLabelArea[i] = col.removeFromTop (12);
        (i == 0 ? pitchSlider : i == 1 ? decaySlider : panSlider).setBounds (col);
    }

    r.removeFromTop (4);
    auto bRow = r.removeFromTop (22);
    const int bw = bRow.getWidth() / 3;
    muteButton.setBounds (bRow.removeFromLeft (bw).reduced (1, 0));
    soloButton.setBounds (bRow.removeFromLeft (bw).reduced (1, 0));
    gateButton.setBounds (bRow.reduced (1, 0));
    r.removeFromTop (6);

    fxArea = r.removeFromBottom (86);
    r.removeFromBottom (6);

    auto mid = r;
    auto left = mid.removeFromLeft (58);
    meterArea = left.removeFromRight (10).reduced (0, 2);
    faderSlider.setBounds (left);

    auto right = mid.reduced (8, 4);
    outBox.setBounds (right.removeFromTop (26));
    right.removeFromTop (8);
    chokeBox.setBounds (right.removeFromTop (26));

    auto fx = fxArea.reduced (6, 4);
    const int fw = fx.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto col = fx.removeFromLeft (fw);
        fxLabelArea[i] = col.removeFromTop (12);
        (i == 0 ? roomSizeSlider : i == 1 ? roomMixSlider : humanizeSlider).setBounds (col);
    }
}

//==============================================================================
namespace
{
    void drawPlaque (Graphics& g, Rectangle<float> r)
    {
        // dark carved-wood plate with a copper rim (like the product box)
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 11.0f);

        ColourGradient wood (Colour (0xff3a2417), 0.0f, r.getY(), Colour (0xff120a06), 0.0f, r.getBottom(), false);
        wood.addColour (0.5, Colour (0xff25150d));
        g.setGradientFill (wood);
        g.fillRoundedRectangle (r, 11.0f);

        // carved pattern: soft diagonal strokes + ember glow
        {
            Graphics::ScopedSaveState ss (g);
            Path clip; clip.addRoundedRectangle (r, 11.0f);
            g.reduceClipRegion (clip);
            g.setColour (Colour (0xffe8923a).withAlpha (0.05f));
            for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 9.0f)
                g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);
            g.setGradientFill (ColourGradient (Colour (0xffe8923a).withAlpha (0.30f), r.getRight() - 40.0f, r.getBottom(),
                                               Colours::transparentBlack, r.getRight() - 40.0f, r.getY(), true));
            g.fillRect (r);
        }

        g.setColour (Colour (0xffe8923a));
        g.drawRoundedRectangle (r, 11.0f, 1.8f);
        g.setColour (Colour (0xffffd9a3).withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (3.0f), 9.0f, 0.8f);
    }
}

//==============================================================================
FxPanel::FxPanel (DrumForgeProcessor& p) : proc (p)
{
    auto pct   = [] (double v) { return String (roundToInt (v * 100.0)) + "%"; };
    auto db    = [] (double v) { return String (v, 1) + " dB"; };
    auto hz    = [] (double v) { return v >= 1000.0 ? String (v / 1000.0, 2) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto ms    = [] (double v) { return v >= 100.0 ? String (roundToInt (v)) + " ms" : String (v, 1) + " ms"; };
    auto ratio = [] (double v) { return String (v, 1) + " : 1"; };

    auto& eq = addCard ("EQUALIZER", "3-band tone shaping", "eqOn");
    addKnob (eq, "eqLow", "LOW", db);
    addKnob (eq, "eqMid", "MID", db);
    addKnob (eq, "eqFreq", "MID FREQ", hz);
    addKnob (eq, "eqHigh", "HIGH", db);

    auto& cp = addCard ("COMPRESSOR", "punch and glue", "compOn");
    addKnob (cp, "compThr", "THRESH", db);
    addKnob (cp, "compRatio", "RATIO", ratio);
    addKnob (cp, "compAtt", "ATTACK", ms);
    addKnob (cp, "compRel", "RELEASE", ms);
    addKnob (cp, "compMakeup", "MAKEUP", db);

    auto& sa = addCard ("SATURATION", "tape-style warmth", "satOn");
    addKnob (sa, "satDrive", "DRIVE", pct);
    addKnob (sa, "satMix", "MIX", pct);

    auto& dl = addCard ("PING-PONG DELAY", "stereo echoes", "dlyOn");
    addKnob (dl, "dlyTime", "TIME", ms);
    addKnob (dl, "dlyFb", "FEEDBACK", pct);
    addKnob (dl, "dlyMix", "MIX", pct);

    auto& rv = addCard ("ROOM REVERB", "space and depth", nullptr);
    addKnob (rv, "roomSize", "SIZE", pct);
    addKnob (rv, "roomDamp", "DAMPING", pct);
    addKnob (rv, "roomMix", "MIX", pct);

    auto& ou = addCard ("OUTPUT & FEEL", "width, human timing, level", nullptr);
    addKnob (ou, "width", "STEREO WIDTH", [] (double v) { return String (roundToInt (v * 100.0)) + "%"; });
    addKnob (ou, "humanize", "HUMANIZE", pct);
    addKnob (ou, "master", "MASTER", db);
}

FxPanel::Card& FxPanel::addCard (const String& title, const String& subtitle, const char* onId)
{
    auto c = std::make_unique<Card>();
    c->title = title;
    c->subtitle = subtitle;

    if (onId != nullptr)
    {
        c->hasSwitch = true;
        c->onButton.setClickingTogglesState (true);
        c->onButton.setColour (TextButton::buttonOnColourId, theme::green);
        c->onButton.setColour (TextButton::textColourOnId, Colours::black);
        c->onButton.setTooltip ("Switch this effect on / off");
        addAndMakeVisible (c->onButton);
        c->att = std::make_unique<ButtonAtt> (proc.apvts, onId, c->onButton);
    }

    cards.push_back (std::move (c));
    return *cards.back();
}

void FxPanel::addKnob (Card& card, const char* paramId, const String& label, std::function<String (double)> fmt)
{
    auto k = std::make_unique<Knob>();
    k->name = label;
    k->slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (Slider::TextBoxBelow, false, 78, 16);
    k->slider.setTextBoxIsEditable (false);
    addAndMakeVisible (k->slider);

    k->att = std::make_unique<SliderAtt> (proc.apvts, paramId, k->slider);

    if (auto* par = proc.apvts.getParameter (paramId))
        k->slider.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));

    k->slider.textFromValueFunction = std::move (fmt);
    k->slider.updateText();

    card.knobs.push_back (k.get());
    knobs.push_back (std::move (k));
}

void FxPanel::refresh()
{
    const float gr = proc.compReduction.load();
    grMeter = jmax (gr, grMeter * 0.85f);

    if (! grArea.isEmpty())
        repaint (grArea);
}

void FxPanel::drawEqCurve (Graphics& g, Rectangle<float> r) const
{
    g.setColour (Colour (0xff14100d));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colour (0xff050709));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    auto inner = r.reduced (6.0f, 6.0f);

    // grid
    g.setColour (Colours::white.withAlpha (0.07f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = inner.getX() + inner.getWidth() * std::log10 (f / 20.0f) / 3.0f;
        g.fillRect (x, inner.getY(), 1.0f, inner.getHeight());
    }
    g.fillRect (inner.getX(), inner.getCentreY(), inner.getWidth(), 1.0f);

    const bool on = proc.apvts.getRawParameterValue ("eqOn")->load() > 0.5f;
    const float lo = proc.apvts.getRawParameterValue ("eqLow")->load();
    const float mid = proc.apvts.getRawParameterValue ("eqMid")->load();
    const float hi = proc.apvts.getRawParameterValue ("eqHigh")->load();
    const float fm = proc.apvts.getRawParameterValue ("eqFreq")->load();

    Path curve;
    const int n = (int) inner.getWidth();

    for (int i = 0; i <= n; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) i / (float) n);
        const float l2 = std::log2 (f / fm);
        const float dbv = lo / (1.0f + std::pow (f / 120.0f, 2.4f))
                        + mid * std::exp (-(l2 * l2) / (2.0f * 0.75f * 0.75f))
                        + hi / (1.0f + std::pow (6500.0f / f, 2.4f));
        const float y = inner.getCentreY() - jlimit (-14.0f, 14.0f, dbv) / 14.0f * inner.getHeight() * 0.5f;

        if (i == 0) curve.startNewSubPath (inner.getX(), y);
        else        curve.lineTo (inner.getX() + (float) i, y);
    }

    Path fill (curve);
    fill.lineTo (inner.getRight(), inner.getCentreY());
    fill.lineTo (inner.getX(), inner.getCentreY());
    fill.closeSubPath();

    const auto c = on ? theme::gold : theme::textDim;
    g.setColour (c.withAlpha (0.18f));
    g.fillPath (fill);
    g.setColour (c);
    g.strokePath (curve, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

void FxPanel::paint (Graphics& g)
{
    for (auto& cp : cards)
    {
        const auto& c = *cp;
        drawPanel (g, c.area.toFloat().reduced (1.0f), 10.0f);

        auto head = c.area.reduced (16, 0).withHeight (46).withY (c.area.getY() + 8);
        label (g, c.title, head.removeFromTop (22), 14.0f, theme::gold, Justification::centredLeft);
        label (g, c.subtitle, head, 10.0f, theme::textDim, Justification::centredLeft);

        if (c.hasSwitch)
        {
            const bool on = c.onButton.getToggleState();
            g.setColour (on ? theme::green : Colour (0xff3a2a2a));
            g.fillEllipse ((float) c.onButton.getX() - 16.0f, (float) c.onButton.getBounds().getCentreY() - 4.0f, 8.0f, 8.0f);
        }

        // separator
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRect (c.area.getX() + 12, c.area.getY() + 56, c.area.getWidth() - 24, 1);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.fillRect (c.area.getX() + 12, c.area.getY() + 57, c.area.getWidth() - 24, 1);
    }

    for (auto& k : knobs)
        label (g, k->name, k->labelArea, 10.0f, theme::goldLight);

    if (! eqArea.isEmpty())
        drawEqCurve (g, eqArea.toFloat());

    if (! grArea.isEmpty())
    {
        auto r = grArea;
        label (g, "GAIN REDUCTION", r.removeFromLeft (110), 9.5f, theme::textDim, Justification::centredLeft);
        g.setColour (Colour (0xff070707));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);

        const float frac = jlimit (0.0f, 1.0f, grMeter / 18.0f);
        auto fillR = r.toFloat().reduced (1.0f);
        fillR.setWidth (fillR.getWidth() * frac);
        g.setGradientFill (ColourGradient (theme::gold, fillR.getX(), 0.0f, theme::red, r.toFloat().getRight(), 0.0f, false));
        g.fillRoundedRectangle (fillR, 2.0f);
        g.setColour (theme::textDim);
        g.setFont (Font (FontOptions (9.5f, Font::bold)));
        g.drawText (String (grMeter, 1) + " dB", r.reduced (6, 0), Justification::centredRight, false);
    }
}

void FxPanel::resized()
{
    const int gap = 12;
    auto area = getLocalBounds().reduced (2, 0);
    const int cw = (area.getWidth() - 2 * gap) / 3;
    const int ch = (area.getHeight() - gap) / 2;

    eqArea = {};
    grArea = {};

    for (size_t i = 0; i < cards.size(); ++i)
    {
        auto& c = *cards[i];
        const int col = (int) i % 3, row = (int) i / 3;
        c.area = Rectangle<int> (area.getX() + col * (cw + gap), area.getY() + row * (ch + gap), cw, ch);

        if (c.hasSwitch)
            c.onButton.setBounds (c.area.getRight() - 16 - 56, c.area.getY() + 14, 56, 24);

        auto body = c.area.withTrimmedTop (62).reduced (12, 8);

        if (i == 0)
        {
            eqArea = body.removeFromTop (juce::jmin (96, body.getHeight() / 3)).withTrimmedBottom (4);
            body.removeFromTop (6);
        }

        if (i == 1)
        {
            grArea = body.removeFromBottom (20);
            body.removeFromBottom (6);
        }

        const int n = (int) c.knobs.size();
        const int cellW = body.getWidth() / jmax (1, n);

        for (int k = 0; k < n; ++k)
        {
            auto cell = Rectangle<int> (body.getX() + k * cellW, body.getY(), cellW, body.getHeight());
            c.knobs[(size_t) k]->labelArea = cell.removeFromTop (16);

            const int size = jmin (cellW - 4, 126, cell.getHeight() - 4);
            c.knobs[(size_t) k]->slider.setBounds (cell.withSizeKeepingCentre (jmax (60, size), jmax (60, size + 18)));
        }
    }
}

//==============================================================================
void GrooveBrowser::ListModel::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= getNumRows())
        return;

    const bool current = isCurrent && isCurrent (row);
    auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (2.0f, 1.0f);

    if (selected || current)
    {
        g.setGradientFill (ColourGradient (theme::gold, 0.0f, r.getY(), theme::goldDark, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);
    }
    else
    {
        g.setColour (Colours::white.withAlpha (row % 2 == 0 ? 0.045f : 0.015f));
        g.fillRoundedRectangle (r, 4.0f);
    }

    g.setColour (selected || current ? Colours::black : Colours::white);
    g.setFont (Font (FontOptions (13.0f, selected || current ? Font::bold : Font::plain)));
    g.drawText (text ? text (row) : String(), r.reduced (10.0f, 0.0f), Justification::centredLeft, true);

    if (detail)
    {
        g.setColour (selected || current ? Colours::black.withAlpha (0.7f) : theme::textDim);
        g.setFont (Font (FontOptions (10.5f)));
        g.drawText (detail (row), r.reduced (10.0f, 0.0f), Justification::centredRight, false);
    }
}

GrooveBrowser::GrooveBrowser (DrumForgeProcessor& p)
    : proc (p), lib (DrumForgeProcessor::getGrooveLibrary()), dragHandle (p)
{
    genreModel.count   = [this] { return (int) lib.size(); };
    genreModel.text    = [this] (int r) { return lib[(size_t) r].name; };
    genreModel.detail  = [this] (int r) { return String ((int) lib[(size_t) r].grooves.size()); };
    genreModel.isCurrent = [this] (int r) { return r == curGenre; };
    genreModel.clicked = [this] (int r) { pickGenre (r); };

    grooveModel.count  = [this] { return (int) filtered.size(); };
    grooveModel.text   = [this] (int r)
    {
        const auto [gi, ri] = filtered[(size_t) r];
        const auto& gr = lib[(size_t) gi].grooves[(size_t) ri];
        return search.getText().trim().isEmpty() ? gr.name : lib[(size_t) gi].name + "  -  " + gr.name;
    };
    grooveModel.detail = [this] (int r)
    {
        const auto [gi, ri] = filtered[(size_t) r];
        return String ((int) lib[(size_t) gi].grooves[(size_t) ri].sections.size()) + " MIDI";
    };
    grooveModel.isCurrent = [this] (int r) { return filtered[(size_t) r] == std::make_pair (curGenre, curGroove); };
    grooveModel.clicked = [this] (int r) { pickGroove (r); };

    sectionModel.count = [this]
    {
        if (! isPositiveAndBelow (curGenre, (int) lib.size()) || ! isPositiveAndBelow (curGroove, (int) lib[(size_t) curGenre].grooves.size()))
            return 0;
        return (int) lib[(size_t) curGenre].grooves[(size_t) curGroove].sections.size();
    };
    sectionModel.text = [this] (int r) { return lib[(size_t) curGenre].grooves[(size_t) curGroove].sections[(size_t) r].label; };
    sectionModel.detail = [this] (int r)
    {
        const auto& res = lib[(size_t) curGenre].grooves[(size_t) curGroove].sections[(size_t) r].resource;
        return (proc.isGroovePlaying() && res == currentResource()) ? String ("PLAYING") : String();
    };
    sectionModel.isCurrent = [this] (int r) { return r == curSection; };
    sectionModel.clicked = [this] (int r) { pickSection (r, true); };

    for (auto* l : { &genreList, &grooveList, &sectionList })
    {
        l->setRowHeight (30);
        l->setColour (ListBox::backgroundColourId, Colours::transparentBlack);
        l->setColour (ListBox::outlineColourId, Colours::transparentBlack);
        addAndMakeVisible (*l);
    }

    genreList.setModel (&genreModel);
    grooveList.setModel (&grooveModel);
    sectionList.setModel (&sectionModel);

    search.setTextToShowWhenEmpty ("Search all grooves (gospel, shuffle, fill...)", Colours::grey);
    search.setFont (Font (FontOptions (13.0f)));
    search.onTextChange = [this] { rebuildGrooves(); };
    addAndMakeVisible (search);

    playButton.setColour (TextButton::buttonColourId, Colour (0xff2f9e5b));
    playButton.setColour (TextButton::buttonOnColourId, Colour (0xff45d27a));
    playButton.onClick = [this] { pickSection (curSection, true); };
    addAndMakeVisible (playButton);

    stopButton.setColour (TextButton::buttonColourId, Colour (0xffc23d36));
    stopButton.onClick = [this] { proc.stopGroove(); };
    addAndMakeVisible (stopButton);

    loopButton.setClickingTogglesState (true);
    loopButton.setToggleState (proc.grooveLoop.load(), dontSendNotification);
    loopButton.setColour (TextButton::buttonOnColourId, theme::goldDark);
    loopButton.onClick = [this] { proc.grooveLoop.store (loopButton.getToggleState()); };
    addAndMakeVisible (loopButton);

    syncButton.setClickingTogglesState (true);
    syncButton.setToggleState (proc.grooveSyncHost.load(), dontSendNotification);
    syncButton.setColour (TextButton::buttonOnColourId, theme::goldDark);
    syncButton.onClick = [this] { proc.grooveSyncHost.store (syncButton.getToggleState()); };
    addAndMakeVisible (syncButton);

    exportButton.onClick = [this] { exportAll(); };
    addAndMakeVisible (exportButton);
    addAndMakeVisible (dragHandle);

    importButton.setTooltip ("Import your own MIDI files (or drop .mid files / folders on this page). They appear under \"My MIDI\".");
    importButton.setColour (TextButton::buttonColourId, Colour (0xff9c5719));
    importButton.onClick = [this] { chooseMidiFiles(); };
    addAndMakeVisible (importButton);

    gmButton.setClickingTogglesState (true);
    gmButton.setToggleState (true, dontSendNotification);
    gmButton.setColour (TextButton::buttonOnColourId, theme::goldDark);
    gmButton.setTooltip ("Imported MIDI uses General MIDI drum notes (38 snare, 42 hi-hat...). ON converts them to the pads of this plugin.");
    gmButton.onClick = [this] { reloadLibrary(); };
    addAndMakeVisible (gmButton);

    for (size_t i = 0; i < lib.size(); ++i)
        if (lib[i].name == "Gospel")
            curGenre = (int) i;

    reloadLibrary();
    dragHandle.setResource (currentResource());
}

void GrooveBrowser::reloadLibrary()
{
    const String keepGenre = isPositiveAndBelow (curGenre, (int) lib.size()) ? lib[(size_t) curGenre].name : String();
    const String keepGroove = (! keepGenre.isEmpty() && isPositiveAndBelow (curGroove, (int) lib[(size_t) curGenre].grooves.size()))
                                  ? lib[(size_t) curGenre].grooves[(size_t) curGroove].name : String();

    lib = DrumForgeProcessor::getGrooveLibrary();

    auto user = DrumForgeProcessor::scanUserMidi (gmButton.getToggleState());
    for (auto& g : user)
    {
        // keep "Fills" last, "My MIDI" just before it
        auto pos = std::find_if (lib.begin(), lib.end(), [] (const GenreEntry& e) { return e.name == "Fills"; });
        lib.insert (pos, std::move (g));
    }

    totalGrooves = totalFiles = 0;
    for (const auto& g : lib)
    {
        totalGrooves += (int) g.grooves.size();
        for (const auto& gr : g.grooves)
            totalFiles += (int) gr.sections.size();
    }

    curGenre = jmin (curGenre, (int) lib.size() - 1);
    for (size_t i = 0; i < lib.size(); ++i)
        if (lib[i].name == keepGenre)
        {
            curGenre = (int) i;
            for (size_t r = 0; r < lib[i].grooves.size(); ++r)
                if (lib[i].grooves[r].name == keepGroove)
                    curGroove = (int) r;
        }

    if (curGenre >= 0 && curGroove >= (int) lib[(size_t) curGenre].grooves.size())
        curGroove = 0;

    genreList.updateContent();
    rebuildGrooves();
    dragHandle.setResource (currentResource());
}

void GrooveBrowser::chooseMidiFiles()
{
    chooser = std::make_unique<FileChooser> ("Choose MIDI files to import",
                                             File::getSpecialLocation (File::userDocumentsDirectory), "*.mid;*.midi");

    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectMultipleItems
                              | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
    {
        Array<File> files = fc.getResults();
        if (! files.isEmpty())
            importFiles (files);
    });
}

int GrooveBrowser::importFiles (const Array<File>& files)
{
    const int n = proc.importMidiFiles (files);
    if (n <= 0)
        return 0;

    curGenre = -1;
    reloadLibrary();

    for (size_t i = 0; i < lib.size(); ++i)
        if (lib[i].name == "My MIDI")
        {
            // select the groove that matches the first imported item
            curGenre = (int) i;
            curGroove = 0;
            const auto first = files.getFirst();
            const auto wanted = first.isDirectory() ? first.getFileName() : first.getFileNameWithoutExtension();

            for (size_t r = 0; r < lib[i].grooves.size(); ++r)
                if (lib[i].grooves[r].name == wanted)
                    curGroove = (int) r;
        }

    curSection = 0;
    search.setText ({}, false);
    rebuildGrooves();
    sectionList.updateContent();
    genreList.scrollToEnsureRowIsOnscreen (jmax (0, curGenre));
    pickSection (0, true);
    return n;
}

bool GrooveBrowser::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
    {
        const File file (f);
        if (file.isDirectory() || file.hasFileExtension ("mid;midi;MID;MIDI"))
            return true;
    }

    return false;
}

void GrooveBrowser::filesDropped (const StringArray& files, int, int)
{
    Array<File> list;
    for (auto& f : files)
        list.add (File (f));

    dropHighlight = false;
    importFiles (list);
    repaint();
}

String GrooveBrowser::currentResource() const
{
    if (! isPositiveAndBelow (curGenre, (int) lib.size()) || ! isPositiveAndBelow (curGroove, (int) lib[(size_t) curGenre].grooves.size()))
        return {};

    const auto& secs = lib[(size_t) curGenre].grooves[(size_t) curGroove].sections;
    return isPositiveAndBelow (curSection, (int) secs.size()) ? secs[(size_t) curSection].resource : String();
}

void GrooveBrowser::rebuildGrooves()
{
    filtered.clear();
    const auto q = search.getText().trim();

    for (int gi = 0; gi < (int) lib.size(); ++gi)
    {
        if (q.isEmpty() && gi != curGenre)
            continue;

        for (int ri = 0; ri < (int) lib[(size_t) gi].grooves.size(); ++ri)
            if (q.isEmpty() || lib[(size_t) gi].name.containsIgnoreCase (q)
                            || lib[(size_t) gi].grooves[(size_t) ri].name.containsIgnoreCase (q))
                filtered.emplace_back (gi, ri);
    }

    grooveList.updateContent();
    grooveList.repaint();
    genreList.repaint();
    sectionList.updateContent();
    repaint();
}

void GrooveBrowser::pickGenre (int gi)
{
    curGenre = gi;
    curGroove = 0;
    curSection = 0;
    search.setText ({}, false);
    rebuildGrooves();
    dragHandle.setResource (currentResource());
    repaint (rollArea);
}

void GrooveBrowser::pickGroove (int row)
{
    if (! isPositiveAndBelow (row, (int) filtered.size()))
        return;

    curGenre = filtered[(size_t) row].first;
    curGroove = filtered[(size_t) row].second;
    curSection = 0;

    genreList.repaint();
    grooveList.repaint();
    sectionList.updateContent();
    sectionList.repaint();
    pickSection (0, true);
}

void GrooveBrowser::pickSection (int index, bool play)
{
    curSection = index;
    const auto res = currentResource();
    dragHandle.setResource (res);

    if (play && res.isNotEmpty())
        proc.startGroove (res, true);

    sectionList.repaint();
    repaint (rollArea);
}

void GrooveBrowser::exportAll()
{
    const auto def = File::getSpecialLocation (File::userDocumentsDirectory)
                         .getChildFile ("African Gospel").getChildFile ("African Drum").getChildFile ("MIDI");
    def.createDirectory();

    chooser = std::make_unique<FileChooser> ("Choose where to export the MIDI library", def);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                          [this] (const FileChooser& fc)
    {
        const auto dir = fc.getResult();
        if (! dir.isDirectory())
            return;

        const int n = proc.exportGrooveLibrary (dir);
        NativeMessageBox::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "African Drum",
                                               String (n) + " MIDI files exported to:\n" + dir.getFullPathName());
    });
}

void GrooveBrowser::refresh()
{
    playButton.setToggleState (proc.isGroovePlaying(), dontSendNotification);
    sectionList.repaint();
    repaint (rollArea.getUnion (bpmArea));
}

void GrooveBrowser::paint (Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat().reduced (1.0f), 10.0f);

    label (g, "MIDI LIBRARY", titleArea, 17.0f, theme::gold, Justification::centredLeft);
    label (g, String ((int) lib.size()) + " genres   |   " + String (totalGrooves) + " grooves   |   " + String (totalFiles)
                 + " MIDI files   |   intro, verse, chorus, bridge, break, fills, full song",
           statsArea, 10.5f, theme::textDim, Justification::centredLeft);

    auto column = [&] (Rectangle<int> head, const String& title, Rectangle<int> listBounds)
    {
        g.setColour (Colour (0xff14100d));
        g.fillRoundedRectangle (listBounds.expanded (3, 3).withTop (head.getY()).toFloat(), 6.0f);
        g.setColour (Colour (0xff050709));
        g.drawRoundedRectangle (listBounds.expanded (3, 3).withTop (head.getY()).toFloat(), 6.0f, 1.0f);
        label (g, title, head, 10.5f, theme::goldLight, Justification::centred);
    };

    column (genreHead, "GENRE", genreList.getBounds());
    column (grooveHead, search.getText().trim().isEmpty() ? String ("GROOVE") : String ("SEARCH RESULTS (" + String ((int) filtered.size()) + ")"), grooveList.getBounds());
    column (sectionHead, "SECTION / FILL", sectionList.getBounds());

    // BPM readout
    g.setGradientFill (ColourGradient (Colour (0xff130f0c), 0.0f, (float) bpmArea.getY(), Colour (0xff2c221c), 0.0f, (float) bpmArea.getBottom(), false));
    g.fillRoundedRectangle (bpmArea.toFloat(), 4.0f);
    g.setColour (theme::gold.withAlpha (0.5f));
    g.drawRoundedRectangle (bpmArea.toFloat(), 4.0f, 1.0f);
    g.setColour (theme::goldLight);
    g.setFont (Font (FontOptions (15.0f, Font::bold)));
    g.drawText (String (roundToInt (proc.getGrooveBpm())) + " BPM", bpmArea, Justification::centred, false);

    drawGrooveRoll (g, proc, rollArea, currentResource());
}

void GrooveBrowser::resized()
{
    auto r = getLocalBounds().reduced (16, 10);

    auto top = r.removeFromTop (34);
    search.setBounds (top.removeFromRight (340).reduced (0, 3));
    titleArea = top.removeFromLeft (170);
    statsArea = top;
    r.removeFromTop (8);

    auto transport = r.removeFromBottom (0);
    (void) transport;

    // piano roll + transport at the bottom, lists above
    const int rollH = jmax (150, r.getHeight() * 36 / 100);
    rollArea = r.removeFromBottom (rollH);
    r.removeFromBottom (8);

    auto bar = r.removeFromBottom (32);
    r.removeFromBottom (8);

    bpmArea = bar.removeFromLeft (84);
    bar.removeFromLeft (12);
    playButton.setBounds (bar.removeFromLeft (76));
    bar.removeFromLeft (6);
    stopButton.setBounds (bar.removeFromLeft (76));
    bar.removeFromLeft (6);
    loopButton.setBounds (bar.removeFromLeft (64));
    bar.removeFromLeft (6);
    syncButton.setBounds (bar.removeFromLeft (92));
    exportButton.setBounds (bar.removeFromRight (112));
    bar.removeFromRight (8);
    dragHandle.setBounds (bar.removeFromRight (170));
    bar.removeFromRight (8);
    importButton.setBounds (bar.removeFromRight (112));
    bar.removeFromRight (6);
    gmButton.setBounds (bar.removeFromRight (76));

    const int gap = 12;
    const int wGenre = r.getWidth() * 22 / 100, wSection = r.getWidth() * 26 / 100;
    const int wGroove = r.getWidth() - wGenre - wSection - 2 * gap;

    auto place = [&] (Rectangle<int> col, Rectangle<int>& head, ListBox& list)
    {
        head = col.removeFromTop (22);
        list.setBounds (col.reduced (3, 3).withTrimmedTop (0));
    };

    place (r.removeFromLeft (wGenre), genreHead, genreList);
    r.removeFromLeft (gap);
    place (r.removeFromLeft (wGroove), grooveHead, grooveList);
    r.removeFromLeft (gap);
    place (r, sectionHead, sectionList);
}

//==============================================================================
// KIT MIXER
KitMixerPanel::KitMixerPanel (DrumForgeProcessor& p) : proc (p)
{
    auto pct   = [] (double v) { return String (roundToInt (v * 100.0)) + "%"; };
    auto db    = [] (double v) { return String (v, 1) + " dB"; };
    auto hz    = [] (double v) { return v >= 1000.0 ? String (v / 1000.0, 2) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto ms    = [] (double v) { return v >= 100.0 ? String (roundToInt (v)) + " ms" : String (v, 1) + " ms"; };
    auto ratio = [] (double v) { return String (v, 1) + ":1"; };

    addKnob (0, "eqLow",      "LOW",      db,    "eqOn");
    addKnob (0, "eqMid",      "MID",      db,    "eqOn");
    addKnob (0, "eqHigh",     "HIGH",     db,    "eqOn");
    addKnob (0, "satDrive",   "DRIVE",    pct,   "satOn");
    addKnob (0, "satMix",     "SAT MIX",  pct,   "satOn");
    addKnob (0, "compThr",    "COMP",     db,    "compOn");
    addKnob (0, "compRatio",  "RATIO",    ratio, "compOn");
    addKnob (0, "compMakeup", "MAKEUP",   db,    "compOn");
    addKnob (0, "master",     "MASTER",   db);

    addKnob (1, "roomSize",   "ROOM",     pct);
    addKnob (1, "roomMix",    "ROOM MIX", pct);
    addKnob (1, "roomDamp",   "DAMPING",  pct);
    addKnob (1, "dlyTime",    "DELAY",    ms,    "dlyOn");
    addKnob (1, "dlyFb",      "FEEDBACK", pct,   "dlyOn");
    addKnob (1, "dlyMix",     "ECHO MIX", pct,   "dlyOn");
    addKnob (1, "width",      "WIDTH",    pct);
    addKnob (1, "humanize",   "HUMANIZE", pct);
    addKnob (1, "eqFreq",     "MID FREQ", hz,    "eqOn");

    for (auto* b : { &toneTab, &spaceTab })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (5160);
        b->setColour (TextButton::buttonOnColourId, Colour (0xffe8923a));
        b->setColour (TextButton::textColourOnId, Colour (0xff1a0e06));
        addAndMakeVisible (*b);
    }

    toneTab.onClick = [this] { showTab (0); };
    spaceTab.onClick = [this] { showTab (1); };
    toneTab.setToggleState (true, dontSendNotification);
    showTab (0);
}

void KitMixerPanel::addKnob (int t, const char* id, const String& name, std::function<String (double)> fmt, const char* fxOnId)
{
    auto k = std::make_unique<K>();
    k->tab = t;
    k->name = name;
    k->s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k->s.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 14);
    k->s.setTextBoxIsEditable (false);
    k->att = std::make_unique<SliderAtt> (proc.apvts, id, k->s);
    k->s.textFromValueFunction = fmt;
    k->s.updateText();

    if (fxOnId != nullptr)
    {
        const String on (fxOnId);
        k->s.onDragStart = [this, on]
        {
            if (auto* prm = proc.apvts.getParameter (on))
                if (prm->getValue() < 0.5f)
                {
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (1.0f);
                    prm->endChangeGesture();
                }
        };
    }

    addChildComponent (k->s);
    knobs.push_back (std::move (k));
}

void KitMixerPanel::showTab (int t)
{
    tab = t;
    toneTab.setToggleState (t == 0, dontSendNotification);
    spaceTab.setToggleState (t == 1, dontSendNotification);

    for (auto& k : knobs)
        k->s.setVisible (k->tab == t);

    repaint();
}

void KitMixerPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    r.removeFromTop (26);
    auto tabs = r.removeFromTop (24);
    toneTab.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2 - 2));
    tabs.removeFromLeft (4);
    spaceTab.setBounds (tabs);
    r.removeFromTop (8);

    const int cw = r.getWidth() / 3, ch = r.getHeight() / 3;
    int idx[2] = { 0, 0 };

    for (auto& k : knobs)
    {
        const int i = idx[k->tab]++;
        Rectangle<int> cell (r.getX() + (i % 3) * cw, r.getY() + (i / 3) * ch, cw, ch);
        k->labelArea = cell.removeFromTop (14);
        k->s.setBounds (cell.reduced (1, 0));
    }
}

void KitMixerPanel::paint (Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat().reduced (1.0f), 8.0f);
    drawPanelTitle (g, Rectangle<int> (0, 4, getWidth(), 24), "KIT MIXER");

    for (auto& k : knobs)
        if (k->tab == tab)
            label (g, k->name, k->labelArea, 9.5f, Colour (0xfff1c88c));
}

//==============================================================================
// PAD SHAPER
PadShaperPanel::PadShaperPanel (DrumForgeProcessor& p) : proc (p)
{
    for (auto* s : { &tuneS, &decayS, &panS, &levelS })
    {
        s->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (Slider::TextBoxBelow, false, 58, 13);
        s->setTextBoxIsEditable (false);
        addAndMakeVisible (*s);
    }

    for (auto* b : { &muteB, &soloB, &gateB })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }

    muteB.setColour (TextButton::buttonOnColourId, theme::red);
    soloB.setColour (TextButton::buttonOnColourId, Colour (0xffe8923a));
    soloB.setColour (TextButton::textColourOnId, Colours::black);
    gateB.setColour (TextButton::buttonOnColourId, theme::gold);
    gateB.setColour (TextButton::textColourOnId, Colours::black);

    loadB.onClick = [this] { showLoadMenu(); };
    loadB.setButtonText ("SWAP");
    loadB.setTooltip ("Swap this piece with the same piece from another kit (cymbals stay the same)");
    clearB.onClick = [this] { proc.clearSample (pad); };
    addAndMakeVisible (loadB);
    addAndMakeVisible (clearB);

    bind (2);
}

void PadShaperPanel::bind (int newPad)
{
    pad = jlimit (0, df::numPads - 1, newPad);
    tuneA.reset(); decayA.reset(); panA.reset(); levelA.reset(); muteA.reset(); soloA.reset(); gateA.reset();

    auto& a = proc.apvts;
    tuneA  = std::make_unique<SliderAtt> (a, df::pid ("pitch", pad), tuneS);
    decayA = std::make_unique<SliderAtt> (a, df::pid ("decay", pad), decayS);
    panA   = std::make_unique<SliderAtt> (a, df::pid ("pan", pad), panS);
    levelA = std::make_unique<SliderAtt> (a, df::pid ("gain", pad), levelS);
    muteA  = std::make_unique<ButtonAtt> (a, df::pid ("mute", pad), muteB);
    soloA  = std::make_unique<ButtonAtt> (a, df::pid ("solo", pad), soloB);
    gateA  = std::make_unique<ButtonAtt> (a, df::pid ("gate", pad), gateB);

    tuneS.setDoubleClickReturnValue (true, 0.0);
    decayS.setDoubleClickReturnValue (true, 5.0);
    panS.setDoubleClickReturnValue (true, 0.0);
    levelS.setDoubleClickReturnValue (true, 0.0);
    tuneS.textFromValueFunction = [] (double v) { return String (v, 1) + " st"; };
    decayS.textFromValueFunction = [] (double v) { return v >= 4.99 ? String ("Full") : String (v, 2) + " s"; };
    panS.textFromValueFunction = [] (double v)
    {
        if (std::abs (v) < 0.005) return String ("C");
        return String (roundToInt (std::abs (v) * 100.0)) + (v < 0 ? " L" : " R");
    };
    levelS.textFromValueFunction = [] (double v) { return String (v, 1) + " dB"; };

    for (auto* s : { &tuneS, &decayS, &panS, &levelS })
        s->updateText();

    repaint();
}

void PadShaperPanel::showLoadMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Swap this piece with:");
    addSamePieceItems (m, 1000, pad);
    if (pad < 9)
        addSwapMenu (m, 1000);
    if (df::externalSamplesEnabled)
    {
        m.addSeparator();
        m.addItem (1, "Load sample from file...");
    }

    Component::SafePointer<PadShaperPanel> safe (this);

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&loadB), [safe] (int result)
    {
        if (safe == nullptr)
            return;

        if (result == 1)
            safe->chooseFile();
        else if (result >= 1000)
            safe->proc.loadFactorySample (safe->pad, swapList()[(size_t) (result - 1000)].resource);
    });
}

void PadShaperPanel::chooseFile()
{
    chooser = std::make_unique<FileChooser> ("Choose a sample for " + DrumForgeProcessor::getPadNames()[pad],
                                             proc.lastDirectory, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    Component::SafePointer<PadShaperPanel> safe (this);

    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe] (const FileChooser& fc)
    {
        if (safe == nullptr)
            return;

        const auto f = fc.getResult();
        if (f.existsAsFile())
            safe->proc.loadSample (safe->pad, f);
    });
}

void PadShaperPanel::refresh()
{
    const auto sn = proc.getSampleName (pad);
    if (sn != lastSample)
    {
        lastSample = sn;
        repaint (nameArea);
    }
}

void PadShaperPanel::paint (Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat().reduced (1.0f), 8.0f);
    drawPanelTitle (g, Rectangle<int> (0, 4, getWidth(), 24), "PAD SHAPER");

    auto t = nameArea.withTrimmedLeft (4).withTrimmedRight (96);
    label (g, DrumForgeProcessor::getPadNames()[pad].toUpperCase() + "  -  "
                 + MidiMessage::getMidiNoteName (df::firstNote + pad, true, true, 3),
           t.removeFromTop (18), 13.0f, Colour (0xfff7e1c4), Justification::centredLeft);

    const auto sn = proc.getSampleName (pad);
    label (g, sn.isEmpty() ? String ("- empty: load a sample -") : sn, t, 10.5f,
           sn.isEmpty() ? theme::textDim : Colour (0xfff1c88c), Justification::centredLeft);

    const char* names[] = { "TUNE", "DECAY", "PAN", "LEVEL" };
    for (int i = 0; i < 4; ++i)
        label (g, names[i], labelArea[i], 9.5f, Colour (0xfff1c88c));
}

void PadShaperPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    r.removeFromTop (26);

    nameArea = r.removeFromTop (38);
    auto btns = nameArea.removeFromRight (92).withSizeKeepingCentre (92, 22);
    loadB.setBounds (btns.removeFromLeft (44));
    btns.removeFromLeft (4);
    clearB.setBounds (btns);
    nameArea = Rectangle<int> (r.getX(), nameArea.getY(), r.getWidth(), 38);

    auto knobs = r.removeFromTop (88);
    const int kw = knobs.getWidth() / 4;
    Slider* sl[] = { &tuneS, &decayS, &panS, &levelS };
    for (int i = 0; i < 4; ++i)
    {
        auto col = knobs.removeFromLeft (kw);
        labelArea[i] = col.removeFromTop (13);
        sl[i]->setBounds (col);
    }

    r.removeFromTop (4);
    auto row = r.removeFromTop (22);
    const int bw = row.getWidth() / 3;
    muteB.setBounds (row.removeFromLeft (bw).reduced (1, 0));
    soloB.setBounds (row.removeFromLeft (bw).reduced (1, 0));
    gateB.setBounds (row.reduced (1, 0));
}

//==============================================================================
// PAD LIST
PadListPanel::PadListPanel (DrumForgeProcessor& p) : proc (p)
{
    list.setModel (this);
    list.setRowHeight (23);
    list.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    list.setColour (ListBox::outlineColourId, Colours::transparentBlack);
    list.setOutlineThickness (0);
    addAndMakeVisible (list);
}

void PadListPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromTop (24);
    list.setBounds (r);
}

void PadListPanel::paint (Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat().reduced (1.0f), 8.0f);
    drawPanelTitle (g, Rectangle<int> (0, 4, getWidth(), 24), "DRUM PADS");
}

void PadListPanel::setSelected (int pad)
{
    selected = pad;
    if (list.getHeight() > 0)
        list.scrollToEnsureRowIsOnscreen (pad);
    list.repaint();
}

void PadListPanel::refresh() { list.repaint(); }

void PadListPanel::paintListBoxItem (int row, Graphics& g, int w, int h, bool)
{
    const auto r = Rectangle<float> (1.0f, 1.0f, (float) w - 2.0f, (float) h - 2.0f);
    const bool sel = row == selected;

    if (sel)
    {
        g.setGradientFill (ColourGradient (Colour (0xffe8923a), 0.0f, r.getY(), Colour (0xff9c5719), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);
    }
    else
    {
        g.setColour (row % 2 ? Colour (0xff1a110b) : Colour (0xff22160e));
        g.fillRoundedRectangle (r, 4.0f);
    }

    const auto ink = sel ? Colour (0xff1a0e06) : Colour (0xfff7e1c4);
    g.setColour (sel ? ink : Colour (0xffe8923a));
    g.setFont (Font (FontOptions (11.0f, Font::bold)));
    g.drawText (String (row + 1) + ".", Rectangle<float> (r.getX() + 6.0f, r.getY(), 22.0f, r.getHeight()), Justification::centredLeft, false);

    g.setColour (ink);
    g.setFont (Font (FontOptions (11.5f, Font::bold)));
    g.drawText (DrumForgeProcessor::getPadNames()[row], Rectangle<float> (r.getX() + 28.0f, r.getY(), 84.0f, r.getHeight()), Justification::centredLeft, true);

    const auto sn = proc.getSampleName (row);
    g.setColour (sel ? ink.withAlpha (0.75f) : (sn.isEmpty() ? Colour (0xff6e5a4a) : Colour (0xffc9b3a0)));
    g.setFont (Font (FontOptions (10.0f)));
    g.drawText (sn.isEmpty() ? String ("- empty -") : sn, Rectangle<float> (r.getX() + 112.0f, r.getY(), r.getWidth() - 134.0f, r.getHeight()),
                Justification::centredLeft, true);

    const float lv = jlimit (0.0f, 1.0f, proc.padLevel[(size_t) row].load() * 1.6f);
    g.setColour ((sel ? Colour (0xff1a0e06) : Colour (0xffffd9a3)).withAlpha (0.25f + 0.75f * lv));
    g.fillEllipse (r.getRight() - 14.0f, r.getCentreY() - 3.5f, 7.0f, 7.0f);
}

//==============================================================================
// PERCUSSION MIXER
PercMixerPanel::PercMixerPanel (DrumForgeProcessor& p) : proc (p)
{
    struct Def { const char* name; std::vector<int> pads; };
    const Def defs[] = { { "KICK", { 0, 1 } }, { "SNARE", { 2, 3, 4 } }, { "TOMS", { 5, 6, 7, 8 } },
                         { "HATS", { 9, 10, 11 } }, { "CYMBALS", { 12, 13, 14, 15, 16, 17, 18, 19, 20 } } };

    auto* gp = dynamic_cast<RangedAudioParameter*> (proc.apvts.getParameter (df::pid ("gain", 0)));
    const auto range = gp->getNormalisableRange();

    for (const auto& d : defs)
    {
        auto g = std::make_unique<Grp>();
        g->name = d.name;
        g->pads = d.pads;

        g->fader.setSliderStyle (Slider::LinearVertical);
        g->fader.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        g->fader.setNormalisableRange (NormalisableRange<double> ((double) range.start, (double) range.end, 0.1, (double) range.skew));
        g->fader.setDoubleClickReturnValue (true, 0.0);
        g->fader.setValue (0.0, dontSendNotification);

        g->pan.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        g->pan.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        g->pan.setRange (-1.0, 1.0, 0.01);
        g->pan.setDoubleClickReturnValue (true, 0.0);
        g->pan.setValue (0.0, dontSendNotification);

        auto* raw = g.get();
        raw->fader.onValueChange = [this, raw] { pushToPads (*raw, "gain", raw->fader.getValue()); repaint(); };
        raw->pan.onValueChange   = [this, raw] { pushToPads (*raw, "pan", raw->pan.getValue()); };

        addAndMakeVisible (g->fader);
        addAndMakeVisible (g->pan);
        groups.push_back (std::move (g));
    }

    // start from the current values of the first pad of each group
    refresh();
}

void PercMixerPanel::pushToPads (Grp& g, const char* base, double value)
{
    for (int pad : g.pads)
        if (auto* prm = proc.apvts.getParameter (df::pid (base, pad)))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) value));
            prm->endChangeGesture();
        }
}

void PercMixerPanel::refresh()
{
    for (auto& g : groups)
    {
        float lv = 0.0f;
        for (int pad : g->pads)
            lv = jmax (lv, proc.padLevel[(size_t) pad].load());
        g->meter = jmax (lv, g->meter * 0.88f);

        const int first = g->pads.front();
        if (! g->fader.isMouseButtonDown())
            if (auto* prm = proc.apvts.getParameter (df::pid ("gain", first)))
                g->fader.setValue (prm->convertFrom0to1 (prm->getValue()), dontSendNotification);
        if (! g->pan.isMouseButtonDown())
            if (auto* prm = proc.apvts.getParameter (df::pid ("pan", first)))
                g->pan.setValue (prm->convertFrom0to1 (prm->getValue()), dontSendNotification);
    }

    repaint();
}

void PercMixerPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    r.removeFromTop (26);
    const int cw = r.getWidth() / (int) groups.size();

    for (auto& g : groups)
    {
        auto col = r.removeFromLeft (cw);
        g->pan.setBounds (col.removeFromTop (40).withSizeKeepingCentre (38, 38));
        auto bottom = col.removeFromBottom (32);
        g->nameArea = bottom.removeFromBottom (14);
        g->dbArea = bottom;
        col.removeFromTop (2);
        auto fa = col.reduced (2, 0);
        g->meterArea = fa.removeFromRight (8).reduced (0, 4);
        g->fader.setBounds (fa);
    }
}

void PercMixerPanel::paint (Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat().reduced (1.0f), 8.0f);
    drawPanelTitle (g, Rectangle<int> (0, 4, getWidth(), 24), "PERCUSSION MIXER");

    for (auto& grp : groups)
    {
        drawSegmentMeter (g, grp->meterArea, grp->meter, Colour (0xffe8923a));
        label (g, String (grp->fader.getValue(), 1), grp->dbArea, 9.5f, Colour (0xfff7e1c4));
        label (g, grp->name, grp->nameArea, 9.0f, Colour (0xfff1c88c));
    }
}

//==============================================================================
DrumForgeEditor::DrumForgeEditor (DrumForgeProcessor& p)
    : AudioProcessorEditor (&p), proc (p), kitPanel (p), drumView (p), kitMixer (p), shaper (p), padList (p), percMixer (p),
      groovePanel (p), fxPanel (p), grooveBrowser (p)
{
    logo = ImageCache::getFromMemory (BinaryData::logo_ag_png, BinaryData::logo_ag_pngSize);
    logoDbl = ImageCache::getFromMemory (BinaryData::logo_dbl_png, BinaryData::logo_dbl_pngSize);
    logoSkm = ImageCache::getFromMemory (BinaryData::logo_skm_png, BinaryData::logo_skm_pngSize);

    setLookAndFeel (&laf);

    prevKitButton.onClick = [this] { kitPanel.stepKit (-1); };
    nextKitButton.onClick = [this] { kitPanel.stepKit (1); };
    prevKitButton.setTooltip ("Previous kit");
    nextKitButton.setTooltip ("Next kit");
    addAndMakeVisible (prevKitButton);
    addAndMakeVisible (nextKitButton);

    for (auto* t : { &performanceTab, &mixerTab, &fxTab, &grooveTab })
    {
        t->setClickingTogglesState (true);
        t->setRadioGroupId (5150);
        t->setColour (TextButton::buttonOnColourId, Colour (0xffe8923a));
        t->setColour (TextButton::textColourOnId, Colour (0xff1a0e06));
        addAndMakeVisible (*t);
    }

    performanceTab.setToggleState (true, dontSendNotification);
    performanceTab.onClick = [this] { showPage (0); };
    mixerTab.onClick = [this] { showPage (1); };
    fxTab.onClick = [this] { showPage (2); };
    grooveTab.onClick = [this] { showPage (3); };

    midiMapButton.setColour (TextButton::buttonColourId, Colour (0xff8a4a14));
    midiMapButton.setTooltip ("MIDI note mapping for e-drum kits: AD2 Standard, General MIDI, Roland, Alesis, Yamaha, ... or MIDI Learn");
    midiMapButton.onClick = [this] { showMidiMapMenu(); };
    addAndMakeVisible (midiMapButton);

    presetsButton.setColour (TextButton::buttonColourId, Colour (0xff8a4a14));
    presetsButton.setTooltip ("Mixer presets for this kit (5 factory presets + your own). Click to choose.");
    presetsButton.onClick = [this] { showMixMenu (&presetsButton); };
    addAndMakeVisible (presetsButton);
    updatePresetButton();

    activateButton.setColour (TextButton::buttonColourId, Colour (0xff9a2a1a));
    activateButton.onClick = [this] { showActivateDialog(); };
    updateActivateButton();
    addChildComponent (activateButton);
    activateButton.setVisible (proc.licenseRequired() && ! proc.isLicensed());

    masterSlider.setSliderStyle (Slider::LinearHorizontal);
    masterSlider.setTextBoxStyle (Slider::TextBoxRight, false, 60, 20);
    masterSlider.setTextBoxIsEditable (false);
    masterSlider.setDoubleClickReturnValue (true, -6.0);
    addAndMakeVisible (masterSlider);
    masterAtt = std::make_unique<SliderAtt> (proc.apvts, "master", masterSlider);
    masterSlider.textFromValueFunction = [] (double v) { return String (v, 1) + " dB"; };
    masterSlider.updateText();

    for (auto* s : { &roomSizeSlider, &roomMixSlider, &humanizeSlider })
    {
        s->setSliderStyle (Slider::LinearHorizontal);
        s->setTextBoxStyle (Slider::TextBoxRight, false, 46, 16);
        s->setTextBoxIsEditable (false);
        addChildComponent (*s);
    }

    roomSizeAtt = std::make_unique<SliderAtt> (proc.apvts, "roomSize", roomSizeSlider);
    roomMixAtt  = std::make_unique<SliderAtt> (proc.apvts, "roomMix", roomMixSlider);
    humanizeAtt = std::make_unique<SliderAtt> (proc.apvts, "humanize", humanizeSlider);

    for (auto* s : { &roomSizeSlider, &roomMixSlider, &humanizeSlider })
    {
        s->textFromValueFunction = [] (double v) { return String (roundToInt (v * 100.0)) + "%"; };
        s->updateText();
    }

    addAndMakeVisible (kitPanel);
    addAndMakeVisible (drumView);
    addAndMakeVisible (kitMixer);
    addAndMakeVisible (shaper);
    addAndMakeVisible (padList);
    addAndMakeVisible (percMixer);
    addAndMakeVisible (groovePanel);
    addChildComponent (fxPanel);
    addChildComponent (grooveBrowser);

    drumView.onPadSelected = [this] (int pad) { shaper.bind (pad); padList.setSelected (pad); };

    padList.onPick = [this] (int pad)
    {
        drumView.setSelectedPad (pad);
        shaper.bind (pad);
        padList.setSelected (pad);
        proc.triggerPad (pad, 0.85f);
    };
    padList.onDoubleClick = [this] (int pad)
    {
        drumView.setSelectedPad (pad);
        shaper.bind (pad);
        padList.setSelected (pad);
        shaper.showLoadMenu();
    };

    drumView.setSelectedPad (2);
    shaper.bind (2);
    padList.setSelected (2);

    for (int i = 0; i < df::numPads; ++i)
    {
        strips[(size_t) i] = std::make_unique<PadStrip> (proc, i);
        addChildComponent (*strips[(size_t) i]);
    }

    // ---- console deck buttons
    for (int gi = 0; gi < numGroups; ++gi)
    {
        const auto& gr = kGroups[gi];
        const int first = gr.first, last = gr.last;

        groupMute[(size_t) gi].setButtonText ("MUTE");
        groupSolo[(size_t) gi].setButtonText ("SOLO");
        groupMute[(size_t) gi].setColour (TextButton::buttonOnColourId, Colour (0xffd23b2e));
        groupSolo[(size_t) gi].setColour (TextButton::buttonOnColourId, Colour (0xffe8a02c));
        groupSolo[(size_t) gi].setColour (TextButton::textColourOnId, Colours::black);
        groupMute[(size_t) gi].setTooltip (String (gr.name) + ": mute / unmute the whole group");
        groupSolo[(size_t) gi].setTooltip (String (gr.name) + ": solo / unsolo the whole group");

        groupMute[(size_t) gi].onClick = [this, first, last] { setPadsBool ("mute", first, last, ! allPadsBool ("mute", first, last)); };
        groupSolo[(size_t) gi].onClick = [this, first, last] { setPadsBool ("solo", first, last, ! allPadsBool ("solo", first, last)); };
        addChildComponent (groupMute[(size_t) gi]);
        addChildComponent (groupSolo[(size_t) gi]);
    }

    clearMutesButton.onClick   = [this] { setPadsBool ("mute", 0, df::numPads - 1, false); };
    clearSolosButton.onClick   = [this] { setPadsBool ("solo", 0, df::numPads - 1, false); };
    resetFadersButton.onClick  = [this] { resetPadsParam ("gain"); };
    resetPansButton.onClick    = [this] { resetPadsParam ("pan"); };
    saveMixButton.setTooltip ("Save the current mixer (levels, pans, mutes, outputs...) for this kit");
    loadMixButton.setTooltip ("Load a saved mixer for this kit");
    saveMixButton.onClick = [this] { saveMixDialog(); };
    loadMixButton.onClick = [this] { showMixMenu(); };

    {
        const char* ids[] = { "micDry", "micStudio", "micRoom", "micHall" };
        const char* tips[] = { "Dry cymbal mic (close, tight)", "Studio cymbal mic (the main one-shot)",
                               "Room cymbal mic (natural ambience)", "Hall cymbal mic (big, long space)" };
        for (int m = 0; m < 4; ++m)
        {
            auto& sl = micSlider[(size_t) m];
            sl.setSliderStyle (Slider::LinearHorizontal);
            sl.setTextBoxStyle (Slider::TextBoxRight, false, 40, 16);
            sl.setTextBoxIsEditable (false);
            sl.setTooltip (tips[m]);
            sl.setDoubleClickReturnValue (true, m == 1 ? 0.85 : 0.0);
            micAtt[(size_t) m] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[m], sl);
            sl.textFromValueFunction = [] (double v) { return String (roundToInt (v * 100.0)) + "%"; };
            sl.updateText();
            addChildComponent (sl);
        }
        sensSlider.setSliderStyle (Slider::LinearHorizontal);
        sensSlider.setTextBoxStyle (Slider::TextBoxRight, false, 40, 16);
        sensSlider.setTextBoxIsEditable (false);
        sensSlider.setTooltip ("Velocity sensitivity: dynamics curve + tone (soft hits are quieter and darker)");
        sensAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "sens", sensSlider);
        sensSlider.textFromValueFunction = [] (double v) { return String (roundToInt (v * 100.0)) + "%"; };
        sensSlider.updateText();
        addChildComponent (sensSlider);
    }

    for (auto* bt : { &clearMutesButton, &clearSolosButton, &resetFadersButton, &resetPansButton, &saveMixButton, &loadMixButton })
        addChildComponent (*bt);

    setResizable (true, true);
    setResizeLimits (900, 585, 1800, 1170);
    getConstrainer()->setFixedAspectRatio (1200.0 / 780.0);
    setSize (1200, 780);

    showPage (0);
    startTimerHz (30);
}

DrumForgeEditor::~DrumForgeEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DrumForgeEditor::showMidiMapMenu()
{
    PopupMenu menu;
    auto ids = std::make_shared<std::vector<String>>();
    const auto cur = proc.getMidiMapId();
    int nextId = 1;

    std::function<void (PopupMenu&, const std::vector<DrumForgeProcessor::MidiMapMenu>&)> build;
    build = [&] (PopupMenu& m, const std::vector<DrumForgeProcessor::MidiMapMenu>& list)
    {
        for (const auto& e : list)
        {
            if (e.separator)
            {
                m.addSeparator();
            }
            else if (! e.children.empty())
            {
                PopupMenu sub;
                build (sub, e.children);
                m.addSubMenu (e.label, sub, true, nullptr, cur.startsWith (e.label + "/"), 0);
            }
            else
            {
                ids->push_back (e.id);
                m.addItem (nextId++, e.label, true, e.id == cur);
            }
        }
    };

    build (menu, DrumForgeProcessor::getMidiMapMenu());

    menu.addSeparator();
    menu.addItem (900, "MIDI Learn: play a note for the selected pad (" + DrumForgeProcessor::getPadNames()[drumView.getSelectedPad()] + ")");
    menu.addItem (901, "Clear learned notes", proc.isMidiMapLearned());

    Component::SafePointer<DrumForgeEditor> safe (this);

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&midiMapButton).withMinimumWidth (190),
                        [safe, ids] (int r)
    {
        if (safe == nullptr || r <= 0)
            return;

        if (r == 900)       safe->proc.startMidiLearn (safe->drumView.getSelectedPad());
        else if (r == 901)  safe->proc.clearMidiLearn();
        else if (r <= (int) ids->size())
            safe->proc.setMidiMap ((*ids)[(size_t) (r - 1)]);

        safe->repaint();
    });
}

void DrumForgeEditor::saveMixDialog()
{
    auto* w = new AlertWindow ("Save mixer", "Name of this mix (saved for the kit \"" + proc.getCurrentKitName() + "\"):",
                               MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", "My Mix");
    w->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    Component::SafePointer<DrumForgeEditor> safe (this);
    w->enterModalState (true, ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (r == 1 && safe != nullptr)
            safe->proc.saveMixPreset (w->getTextEditorContents ("name"));
    }), true);
}

void DrumForgeEditor::updatePresetButton()
{
    if (currentPresetKit != proc.getCurrentKitName())
    {
        currentPresetKit = proc.getCurrentKitName();
        currentPreset = {};
    }
    presetsButton.setButtonText (currentPreset.isEmpty() ? String ("PRESETS") : "PRESET: " + currentPreset.substring (currentPreset.indexOfChar (' ') >= 0 && currentPreset[0] >= '0' && currentPreset[0] <= '9' ? 2 : 0));
}

void DrumForgeEditor::showMixMenu (Component* target)
{
    const auto names = proc.listMixPresets();
    PopupMenu m, del;

    if (target == nullptr)
        target = &loadMixButton;
    else
    {
        m.addSectionHeader ("Mixer presets - " + proc.getCurrentKitName());
        m.addItem (5000, "Save current mix as preset...");
        m.addSeparator();
    }

    if (names.isEmpty())
        m.addItem (-1, "No saved mixes for this kit yet", false);

    for (int i = 0; i < names.size(); ++i)
    {
        m.addItem (1 + i, names[i], true, names[i] == currentPreset);
        del.addItem (1000 + i, names[i]);
    }

    if (! names.isEmpty())
    {
        m.addSeparator();
        m.addSubMenu ("Delete", del);
    }

    Component::SafePointer<DrumForgeEditor> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (target).withMinimumWidth (220), [safe, names] (int r)
    {
        if (safe == nullptr || r <= 0)
            return;

        if (r == 5000)
            safe->saveMixDialog();
        else if (r >= 1000)
            safe->proc.deleteMixPreset (names[r - 1000]);
        else if (safe->proc.loadMixPreset (names[r - 1]))
        {
            safe->currentPreset = names[r - 1];
            safe->currentPresetKit = safe->proc.getCurrentKitName();
            safe->updatePresetButton();
        }
    });
}

void DrumForgeEditor::setPadsBool (const String& param, int first, int last, bool value)
{
    for (int i = first; i <= last; ++i)
        if (auto* p = proc.apvts.getParameter (df::pid (param.toRawUTF8(), i)))
            p->setValueNotifyingHost (value ? 1.0f : 0.0f);
}

void DrumForgeEditor::resetPadsParam (const String& param)
{
    for (int i = 0; i < df::numPads; ++i)
        if (auto* p = proc.apvts.getParameter (df::pid (param.toRawUTF8(), i)))
            p->setValueNotifyingHost (p->getDefaultValue());
}

bool DrumForgeEditor::allPadsBool (const String& param, int first, int last) const
{
    for (int i = first; i <= last; ++i)
        if (auto* p = proc.apvts.getParameter (df::pid (param.toRawUTF8(), i)))
            if (p->getValue() < 0.5f)
                return false;
    return true;
}

void DrumForgeEditor::showPage (int newPage)
{
    page = newPage;
    const bool perf = page == 0, mixer = page == 1;

    kitPanel.setVisible (perf);
    drumView.setVisible (perf);
    kitMixer.setVisible (perf);
    shaper.setVisible (perf);
    padList.setVisible (perf);
    percMixer.setVisible (perf);
    groovePanel.setVisible (perf);
    prevKitButton.setVisible (perf);
    nextKitButton.setVisible (perf);
    fxPanel.setVisible (page == 2);
    grooveBrowser.setVisible (page == 3);

    for (auto& s : strips)
        s->setVisible (mixer);

    for (int gi = 0; gi < numGroups; ++gi)
    {
        groupMute[(size_t) gi].setVisible (mixer);
        groupSolo[(size_t) gi].setVisible (mixer);
    }
    for (auto* bt : { &clearMutesButton, &clearSolosButton, &resetFadersButton, &resetPansButton, &saveMixButton, &loadMixButton })
        bt->setVisible (mixer);
    for (auto& sl : micSlider)
        sl.setVisible (mixer);
    sensSlider.setVisible (mixer);

    for (auto* s : { &roomSizeSlider, &roomMixSlider, &humanizeSlider })
        s->setVisible (mixer);

    performanceTab.setToggleState (perf, dontSendNotification);
    mixerTab.setToggleState (mixer, dontSendNotification);
    fxTab.setToggleState (page == 2, dontSendNotification);
    grooveTab.setToggleState (page == 3, dontSendNotification);
    resized();
    repaint();
}

void DrumForgeEditor::timerCallback()
{
    updatePresetButton();
    if (page == 0)
    {
        kitPanel.refresh();
        drumView.refresh();
        shaper.refresh();
        padList.refresh();
        percMixer.refresh();
        groovePanel.refresh();
    }
    else if (page == 1)
    {
        for (auto& s : strips)
            s->refresh();

        for (int gi = 0; gi < numGroups; ++gi)
        {
            groupMute[(size_t) gi].setToggleState (allPadsBool ("mute", kGroups[gi].first, kGroups[gi].last), dontSendNotification);
            groupSolo[(size_t) gi].setToggleState (allPadsBool ("solo", kGroups[gi].first, kGroups[gi].last), dontSendNotification);
        }
    }
    else if (page == 2)
        fxPanel.refresh();
    else
        grooveBrowser.refresh();

    const auto kit = proc.getCurrentKitName();
    if (kit != lastKitName)
    {
        lastKitName = kit;
        repaint (kitNavArea);
    }

    String cap;
    if (proc.getLearnPad() >= 0)
        cap = "LEARN: hit a pad for " + DrumForgeProcessor::getPadNames()[proc.getLearnPad()].toUpperCase();
    else
        cap = DrumForgeProcessor::midiMapLabel (proc.getMidiMapId()) + (proc.isMidiMapLearned() ? " + learned" : "");

    if (cap != lastMapCaption)
    {
        lastMapCaption = cap;
        repaint (midiMapCaption);
    }

    meterL = jmax (proc.masterPeakL.load(), meterL * 0.9f);
    meterR = jmax (proc.masterPeakR.load(), meterR * 0.9f);
    repaint (masterMeterArea);
}

void DrumForgeEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds();
    const float s = (float) b.getWidth() / 1200.0f;
    auto S = [s] (int v) { return roundToInt ((float) v * s); };

    // ---- carved dark backdrop with an ember glow
    g.setGradientFill (ColourGradient (Colour (0xff22150c), 0.0f, 0.0f, Colour (0xff070302), 0.0f, (float) b.getHeight(), false));
    g.fillAll();

    {
        g.setColour (Colour (0xffe8923a).withAlpha (0.030f));
        const float step = 15.0f * s;
        for (float x = -(float) b.getHeight(); x < (float) b.getWidth(); x += step)
            g.drawLine (x, (float) b.getHeight(), x + (float) b.getHeight(), 0.0f, 1.0f);
        g.setColour (Colour (0xff000000).withAlpha (0.10f));
        for (float x = -(float) b.getHeight() + step * 0.5f; x < (float) b.getWidth(); x += step)
            g.drawLine (x, 0.0f, x + (float) b.getHeight(), (float) b.getHeight(), 1.0f);
    }

    g.setGradientFill (ColourGradient (Colour (0xffd9772a).withAlpha (0.26f), (float) b.getWidth() * 0.5f, (float) S (40),
                                       Colours::transparentBlack, (float) b.getWidth() * 0.5f, (float) S (40) + (float) b.getWidth() * 0.55f, true));
    g.fillAll();
    g.setGradientFill (ColourGradient (Colour (0xffd9772a).withAlpha (0.18f), 0.0f, (float) b.getHeight(),
                                       Colours::transparentBlack, (float) S (420), (float) b.getHeight() - (float) S (150), true));
    g.fillAll();
    g.setGradientFill (ColourGradient (Colour (0xffd9772a).withAlpha (0.18f), (float) b.getWidth(), (float) b.getHeight(),
                                       Colours::transparentBlack, (float) b.getWidth() - (float) S (420), (float) b.getHeight() - (float) S (150), true));
    g.fillAll();

    // outer frame
    g.setColour (Colour (0xffb8651f));
    g.drawRect (b.toFloat().reduced (2.0f), 1.6f);
    g.setColour (Colour (0xffe8923a).withAlpha (0.25f));
    g.drawRect (b.toFloat().reduced (6.0f), 0.8f);

    // ---- header: AG logo window, title, tagline
    {
        auto pl = plaqueArea.toFloat();
        g.setGradientFill (ColourGradient (Colour (0xff3a2417), 0.0f, pl.getY(), Colour (0xff120a06), 0.0f, pl.getBottom(), false));
        g.fillRoundedRectangle (pl, 9.0f);
        g.setColour (Colour (0xffe8923a));
        g.drawRoundedRectangle (pl, 9.0f, 1.8f);

        Path clip;
        clip.addRoundedRectangle (logoArea.toFloat(), 6.0f);
        g.setColour (Colours::black);
        g.fillPath (clip);

        if (logo.isValid())
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (clip);
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            g.drawImage (logo, logoArea.toFloat(), RectanglePlacement::stretchToFit);
        }

        g.setColour (Colour (0xfff1b070));
        g.strokePath (clip, PathStrokeType (1.2f));
    }

    {
        const auto tx = Rectangle<int> (S (170), 0, S (870), S (96));

        g.setColour (Colour (0xfff1b070));
        g.setFont (Font (FontOptions (13.0f * s, Font::bold)).withExtraKerningFactor (0.16f));
        g.drawText ("THE SOUND OF GOSPEL DRUMS", tx.withTop (S (7)).withHeight (S (18)), Justification::centred, false);

        const auto titleR = tx.withTop (S (22)).withHeight (S (56));
        g.setFont (Font (FontOptions (54.0f * s, Font::bold)).withExtraKerningFactor (0.03f));
        g.setColour (Colour (0xffe8923a).withAlpha (0.45f));
        g.drawText ("AFRICAN DRUM", titleR.translated (0, S (3)), Justification::centred, false);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawText ("AFRICAN DRUM", titleR.translated (S (2), S (2)), Justification::centred, false);
        g.setGradientFill (ColourGradient (Colour (0xffffffff), 0.0f, (float) titleR.getY(), Colour (0xffe3b58a), 0.0f, (float) titleR.getBottom(), false));
        g.drawText ("AFRICAN DRUM", titleR, Justification::centred, false);

        g.setColour (Colour (0xffe9c9a4));
        g.setFont (Font (FontOptions (12.5f * s, Font::bold)).withExtraKerningFactor (0.10f));
        g.drawText ("AFRICAN DRUM KIT VIRTUAL INSTRUMENT", tx.withTop (S (76)).withHeight (S (16)), Justification::centred, false);
    }

    g.setColour (Colour (0xffb8651f).withAlpha (0.7f));
    g.drawHorizontalLine (S (94), (float) S (14), (float) b.getWidth() - (float) S (14));

    // ---- MIDI map caption
    for (auto pr : { std::make_pair (&logoDbl, logoDblArea), std::make_pair (&logoSkm, logoSkmArea) })
        if (pr.first->isValid())
        {
            Path clip;
            clip.addRoundedRectangle (pr.second.toFloat(), 4.0f);
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (clip);
            g.setColour (Colours::black);
            g.fillRect (pr.second);
            g.drawImage (*pr.first, pr.second.reduced (2).toFloat(), RectanglePlacement::centred);
            g.setColour (Colour (0xff5a3414));
            g.drawRoundedRectangle (pr.second.toFloat().reduced (0.5f), 4.0f, 1.0f);
        }

    label (g, lastMapCaption.isEmpty() ? String ("Default") : lastMapCaption, midiMapCaption, 9.0f * s,
           proc.getLearnPad() >= 0 ? Colour (0xffffe08a) : Colour (0xfff1c88c));

    // ---- kit navigator (Performance)
    if (page == 0)
    {
        auto k = kitNavArea;
        g.setGradientFill (ColourGradient (Colour (0xff0c0705), 0.0f, (float) k.getY(), Colour (0xff25160d), 0.0f, (float) k.getBottom(), false));
        g.fillRoundedRectangle (k.toFloat(), 7.0f);
        g.setColour (Colour (0xffb8651f));
        g.drawRoundedRectangle (k.toFloat(), 7.0f, 1.4f);

        label (g, lastKitName.isEmpty() ? String ("NO KIT LOADED  -  load your samples") : lastKitName.toUpperCase(),
               k.reduced (S (40), 0), 14.0f * s, Colour (0xfff7e1c4));
    }

    // ---- master
    label (g, "MASTER", masterLabelArea, 9.5f * s, Colour (0xfff1c88c), Justification::centredRight);
    drawSegmentMeter (g, Rectangle<int> (masterMeterArea.getX(), masterMeterArea.getY(), masterMeterArea.getWidth() / 2 - 1, masterMeterArea.getHeight()), meterL, Colour (0xffe8923a));
    drawSegmentMeter (g, Rectangle<int> (masterMeterArea.getCentreX() + 1, masterMeterArea.getY(), masterMeterArea.getWidth() / 2 - 1, masterMeterArea.getHeight()), meterR, Colour (0xffe8923a));

    // frame around the drum view
    if (page == 0)
    {
        g.setColour (Colour (0xffb8651f));
        g.drawRoundedRectangle (stageArea.toFloat().reduced (0.5f), 8.0f, 1.6f);
        g.setColour (Colour (0xffe8923a).withAlpha (0.25f));
        g.drawRoundedRectangle (stageArea.toFloat().reduced (3.0f), 6.0f, 0.8f);
    }

    if (page == 1 && ! consoleArea.isEmpty())
    {
        // black console bezel
        auto bz = consoleArea.toFloat();
        g.setGradientFill (ColourGradient (Colour (0xff1c1612), 0.0f, bz.getY(), Colour (0xff050403), 0.0f, bz.getBottom(), false));
        g.fillRoundedRectangle (bz, 12.0f);
        g.setColour (Colour (0xffb8651f));
        g.drawRoundedRectangle (bz.reduced (0.5f), 12.0f, 1.6f);
        g.setColour (Colour (0xffe8923a).withAlpha (0.22f));
        g.drawRoundedRectangle (bz.reduced (3.5f), 10.0f, 0.8f);

        // title bar
        label (g, "AFRICAN DRUM  -  MIXER CONSOLE", consoleTitleArea.withTrimmedLeft (4), 11.0f * s, Colour (0xfff7e1c4),
               Justification::centredLeft);
        label (g, "21 CHANNELS  |  16 STEREO OUTPUTS  |  AFRICAN GOSPEL", consoleTitleArea.withTrimmedRight (4), 9.5f * s,
               Colour (0xffb8956a), Justification::centredRight);

        // coloured group bands
        for (int gi = 0; gi < numGroups; ++gi)
        {
            const auto t = kGroups[gi].tint;
            auto br = groupBands[(size_t) gi].toFloat();
            g.setGradientFill (ColourGradient (t.brighter (0.25f), 0.0f, br.getY(), t.darker (0.55f), 0.0f, br.getBottom(), false));
            g.fillRoundedRectangle (br, 3.0f);
            g.setColour (Colours::black.withAlpha (0.5f));
            g.drawRoundedRectangle (br, 3.0f, 1.0f);
            label (g, kGroups[gi].name, br.toNearestInt(), 10.0f * s, Colours::white);
        }

        // control deck
        auto dk = deckArea.toFloat();
        g.setGradientFill (ColourGradient (Colour (0xff17110d), 0.0f, dk.getY(), Colour (0xff0a0705), 0.0f, dk.getBottom(), false));
        g.fillRoundedRectangle (dk, 8.0f);
        g.setColour (Colour (0xff5a3414));
        g.drawRoundedRectangle (dk.reduced (0.5f), 8.0f, 1.2f);

        for (int gi = 0; gi < numGroups; ++gi)
        {
            auto blk = groupBlocks[(size_t) gi];
            const auto t = kGroups[gi].tint;
            auto chip = blk.removeFromTop (S (18));
            g.setColour (t.withAlpha (0.9f));
            g.fillRoundedRectangle (chip.toFloat(), 3.0f);
            label (g, kGroups[gi].name, chip, 10.0f * s, Colours::black.withAlpha (0.85f));
        }

        label (g, "ROOM SIZE", roomLabelArea, 9.5f * s, Colour (0xfff1c88c), Justification::centredRight);
        label (g, "ROOM MIX", mixLabelArea, 9.5f * s, Colour (0xfff1c88c), Justification::centredRight);
        label (g, "HUMANIZE", humLabelArea, 9.5f * s, Colour (0xfff1c88c), Justification::centredRight);
        label (g, "SENSITIVITY", sensLabelArea, 9.5f * s, Colour (0xfff1c88c), Justification::centredRight);
        {
            const char* mn[] = { "MIC DRY", "MIC STUDIO", "MIC ROOM", "MIC HALL" };
            for (int m = 0; m < 4; ++m)
                label (g, mn[m], micLabelArea[(size_t) m], 9.0f * s, Colour (0xff9fd6ff), Justification::centredRight);
        }
    }
}

void DrumForgeEditor::updateActivateButton()
{
    activateButton.setButtonText ("ACTIVATE");
    activateButton.setTooltip ("Demo mode: the sound mutes for 3 s every 40 s. Click to enter your licence key.");
}

void DrumForgeEditor::showActivateDialog()
{
    auto* w = new AlertWindow ("Activate African Drum", "Enter the name, e-mail and key you received.", MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", "", "Name");
    w->addTextEditor ("email", "", "E-mail");
    w->addTextEditor ("key", "", "Licence key (AGD-XXXXX-XXXXX-XXXXX-XXXXX-XXXXX)");
    w->addButton ("Activate", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    Component::SafePointer<DrumForgeEditor> safe (this);
    w->enterModalState (true, ModalCallbackFunction::create ([safe, w] (int r)
    {
        std::unique_ptr<AlertWindow> own (w);
        if (r != 1 || safe == nullptr)
            return;

        const bool ok = safe->proc.activateLicense (w->getTextEditorContents ("name"),
                                                    w->getTextEditorContents ("email"),
                                                    w->getTextEditorContents ("key"));
        if (ok)
            safe->activateButton.setVisible (false);

        AlertWindow::showMessageBoxAsync (ok ? MessageBoxIconType::InfoIcon : MessageBoxIconType::WarningIcon,
                                          "African Drum", ok ? "Thank you! African Drum is activated." : "That key does not match the name and e-mail.");
    }), false);
}

void DrumForgeEditor::resized()
{
    const auto b = getLocalBounds();
    const float s = (float) b.getWidth() / 1200.0f;
    auto S = [s] (int v) { return roundToInt ((float) v * s); };

    // header
    plaqueArea = Rectangle<int> (S (14), S (8), S (142), S (82));
    logoArea = Rectangle<int> (plaqueArea.getX() + S (7), plaqueArea.getY() + S (4), S (128), S (74));
    midiMapButton.setBounds (S (1056), S (12), S (130), S (30));
    activateButton.setBounds (S (930), S (12), S (118), S (30));
    midiMapCaption = Rectangle<int> (S (1046), S (46), S (150), S (16));
    logoSkmArea = Rectangle<int> (S (1040), S (62), S (72), S (36));
    logoDblArea = Rectangle<int> (S (1114), S (62), S (72), S (36));

    // tab row
    performanceTab.setBounds (S (14), S (100), S (124), S (28));
    mixerTab.setBounds (S (144), S (100), S (84), S (28));
    fxTab.setBounds (S (234), S (100), S (92), S (28));
    grooveTab.setBounds (S (332), S (100), S (84), S (28));
    presetsButton.setBounds (S (426), S (100), S (230), S (28));

    masterLabelArea = Rectangle<int> (S (830), S (100), S (66), S (28));
    masterMeterArea = Rectangle<int> (S (902), S (102), S (26), S (24));
    masterSlider.setBounds (S (934), S (100), S (252), S (28));

    const bool perf = page == 0;
    const int topY = S (136);

    if (page >= 2)
    {
        const auto a = b.withTrimmedTop (topY).reduced (S (14), S (6));
        fxPanel.setBounds (page == 2 ? a : Rectangle<int>());
        grooveBrowser.setBounds (page == 3 ? a : Rectangle<int>());
        for (auto* c : std::initializer_list<Component*> { &kitPanel, &drumView, &kitMixer, &shaper, &padList, &percMixer, &groovePanel })
            c->setBounds ({});
        for (auto& st : strips)
            st->setBounds ({});
        footerArea = {};
        kitNavArea = stageArea = {};
        return;
    }

    fxPanel.setBounds ({});
    grooveBrowser.setBounds ({});

    if (perf)
    {
        const int topH = S (388), gap = S (8);
        const int botY = topY + topH + gap;
        const int botH = b.getHeight() - botY - S (10);
        const int lx = S (14), lw = S (262), cx = S (284), cw = S (632), rx = S (924), rw = S (262);

        kitMixer.setBounds (lx, topY, lw, topH);

        kitNavArea = Rectangle<int> (cx, topY, cw, S (28));
        prevKitButton.setBounds (kitNavArea.getX() + S (5), kitNavArea.getY() + S (3), S (28), S (22));
        nextKitButton.setBounds (kitNavArea.getRight() - S (33), kitNavArea.getY() + S (3), S (28), S (22));
        stageArea = Rectangle<int> (cx, topY + S (34), cw, topH - S (34));
        drumView.setBounds (stageArea.reduced (3));

        const int shH = S (194);
        shaper.setBounds (rx, topY, rw, shH);
        kitPanel.setBounds (rx, topY + shH + gap, rw, topH - shH - gap);

        padList.setBounds (lx, botY, lw, botH);
        groovePanel.setBounds (cx, botY, cw, botH);
        percMixer.setBounds (rx, botY, rw, botH);

        footerArea = {};
        for (auto& st : strips)
            st->setBounds ({});
        return;
    }

    // ---- mixer page: console
    kitNavArea = stageArea = {};
    consoleArea = b.withTrimmedTop (topY).reduced (S (10), S (2)).withTrimmedBottom (S (8));
    auto inner = consoleArea.reduced (S (8), S (6));
    consoleTitleArea = inner.removeFromTop (S (20));
    inner.removeFromTop (S (3));
    auto bandRow = inner.removeFromTop (S (18));
    inner.removeFromTop (S (2));

    deckArea = inner.removeFromBottom (S (96));
    inner.removeFromBottom (S (6));
    footerArea = {};

    // 21 channel strips side by side
    const int n = df::numPads;
    const float sw = (float) inner.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const int x0 = inner.getX() + roundToInt (sw * (float) i);
        const int x1 = inner.getX() + roundToInt (sw * (float) (i + 1));
        // toms are shown High -> Mid -> Low -> Floor (pads 9, 8, 7, 6)
        const int padAtSlot = (i >= 5 && i <= 8) ? 13 - i : i;
        strips[(size_t) padAtSlot]->setBounds (x0, inner.getY(), x1 - x0, inner.getHeight());
    }

    // group colour bands above the strips
    for (int gi = 0; gi < numGroups; ++gi)
    {
        const int x0 = inner.getX() + roundToInt (sw * (float) kGroups[gi].first);
        const int x1 = inner.getX() + roundToInt (sw * (float) (kGroups[gi].last + 1));
        groupBands[(size_t) gi] = Rectangle<int> (x0 + 1, bandRow.getY(), x1 - x0 - 2, bandRow.getHeight());
    }

    // control deck: group buttons | actions | room, humanize, sensitivity | cymbal mics
    auto d = deckArea.reduced (S (10), S (8));
    deckGroupsArea = d.removeFromLeft (S (400));
    const int blockW = deckGroupsArea.getWidth() / numGroups;
    for (int gi = 0; gi < numGroups; ++gi)
    {
        auto blk = deckGroupsArea.withX (deckGroupsArea.getX() + gi * blockW).withWidth (blockW).reduced (S (4), 0);
        groupBlocks[(size_t) gi] = blk;
        blk.removeFromTop (S (22));
        const int bh = (blk.getHeight() - S (4)) / 2;
        groupMute[(size_t) gi].setBounds (blk.removeFromTop (bh));
        blk.removeFromTop (S (4));
        groupSolo[(size_t) gi].setBounds (blk.removeFromTop (bh));
    }

    d.removeFromLeft (S (12));
    auto act = d.removeFromLeft (S (240));
    const int ah = (act.getHeight() - S (6)) / 2, aw = (act.getWidth() - S (12)) / 3;
    auto ar1 = act.removeFromTop (ah);
    act.removeFromTop (S (6));
    auto ar2 = act;
    auto place = [&] (Rectangle<int>& row, TextButton& bt, bool last)
    {
        bt.setBounds (last ? row : row.removeFromLeft (aw));
        if (! last)
            row.removeFromLeft (S (6));
    };
    place (ar1, clearMutesButton, false);  place (ar1, clearSolosButton, false);  place (ar1, saveMixButton, true);
    place (ar2, resetFadersButton, false); place (ar2, resetPansButton, false);   place (ar2, loadMixButton, true);

    d.removeFromLeft (S (12));
    const int rh = d.getHeight() / 4;
    auto colA = d.removeFromLeft (d.getWidth() / 2 - S (6));
    d.removeFromLeft (S (12));
    auto colB = d;
    auto rr1 = colA.removeFromTop (rh), rr2 = colA.removeFromTop (rh), rr3 = colA.removeFromTop (rh), rr4 = colA;
    roomLabelArea = rr1.removeFromLeft (S (74));
    roomSizeSlider.setBounds (rr1.reduced (S (4), 0));
    mixLabelArea = rr2.removeFromLeft (S (74));
    roomMixSlider.setBounds (rr2.reduced (S (4), 0));
    humLabelArea = rr3.removeFromLeft (S (74));
    humanizeSlider.setBounds (rr3.reduced (S (4), 0));
    sensLabelArea = rr4.removeFromLeft (S (74));
    sensSlider.setBounds (rr4.reduced (S (4), 0));
    for (int m = 0; m < 4; ++m)
    {
        auto row = colB.removeFromTop (rh);
        micLabelArea[(size_t) m] = row.removeFromLeft (S (74));
        micSlider[(size_t) m].setBounds (row.reduced (S (4), 0));
    }

    for (auto* c : std::initializer_list<Component*> { &kitPanel, &drumView, &kitMixer, &shaper, &padList, &percMixer, &groovePanel })
        c->setBounds ({});
}
