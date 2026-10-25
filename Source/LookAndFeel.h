#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// African Gospel visual identity: black + gold
namespace theme
{
    const juce::Colour gold      (0xffe8923a);
    const juce::Colour goldLight (0xffffd9a3);
    const juce::Colour goldDark  (0xffa85f1d);
    const juce::Colour bg        (0xff281f19);
    const juce::Colour panelTop  (0xff544135);
    const juce::Colour panelBot  (0xff352921);
    const juce::Colour control   (0xff634d3f);
    const juce::Colour outline   (0xff15110d);
    const juce::Colour textDim   (0xffe7b392);
    const juce::Colour green     (0xff45d27a);
    const juce::Colour red       (0xffee4b40);
}

class DblLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DblLookAndFeel()
    {
        using namespace juce;
        setColour (ResizableWindow::backgroundColourId, theme::bg);
        setColour (TextButton::buttonColourId, theme::control);
        setColour (TextButton::textColourOffId, Colours::white);
        setColour (TextButton::textColourOnId, Colours::white);
        setColour (ComboBox::backgroundColourId, theme::control);
        setColour (ComboBox::outlineColourId, theme::outline);
        setColour (ComboBox::textColourId, Colours::white);
        setColour (ComboBox::arrowColourId, theme::gold);
        setColour (PopupMenu::backgroundColourId, Colour (0xff382b23));
        setColour (ScrollBar::thumbColourId, theme::goldDark);
        setColour (ScrollBar::trackColourId, Colours::transparentBlack);
        setColour (PopupMenu::textColourId, Colours::white);
        setColour (PopupMenu::headerTextColourId, theme::gold);
        setColour (PopupMenu::highlightedBackgroundColourId, theme::gold);
        setColour (PopupMenu::highlightedTextColourId, Colours::black);
        setColour (Slider::textBoxTextColourId, Colour (0xffe6e6e6));
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (TextEditor::backgroundColourId, Colour (0xff0c0c0c));
        setColour (TextEditor::textColourId, Colours::white);
        setColour (TextEditor::highlightColourId, theme::goldDark);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace juce;
        auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float trackW = 3.0f;
        const float arcR = radius - trackW * 0.5f;

        Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (Colour (0xff070707));
        g.strokePath (track, PathStrokeType (trackW + 1.5f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colour (0xff333333));
        g.strokePath (track, PathStrokeType (trackW, PathStrokeType::curved, PathStrokeType::rounded));

        // Value arc: from the centre for bipolar knobs, from the start otherwise
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float fromAngle = bipolar ? startAngle + 0.5f * (endAngle - startAngle) : startAngle;

        Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (fromAngle, angle), jmax (fromAngle, angle), true);
        g.setColour (s.isEnabled() ? theme::gold : theme::textDim);
        g.strokePath (val, PathStrokeType (trackW, PathStrokeType::curved, PathStrokeType::rounded));

        const float kr = radius - 6.0f;
        if (kr > 4.0f)
        {
            g.setColour (Colours::black.withAlpha (0.55f));
            g.fillEllipse (c.x - kr, c.y - kr + 1.5f, kr * 2.0f, kr * 2.0f);

            g.setGradientFill (ColourGradient (Colour (0xfff0b070), c.x - kr, c.y - kr, Colour (0xff6a3a14), c.x + kr, c.y + kr, false));
            g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
            const float ki = kr - 3.0f;
            g.setGradientFill (ColourGradient (Colour (0xff3a2a20), c.x, c.y - ki, Colour (0xff0f0905), c.x, c.y + ki, false));
            g.fillEllipse (c.x - ki, c.y - ki, ki * 2.0f, ki * 2.0f);
            g.setColour (Colour (0xff050302));
            g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

            Path p;
            p.addRoundedRectangle (-1.6f, -kr + 2.5f, 3.2f, kr * 0.5f, 1.4f);
            p.applyTransform (AffineTransform::rotation (angle).translated (c.x, c.y));
            g.setColour (theme::goldLight);
            g.fillPath (p);
        }
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                           float, float, juce::Slider::SliderStyle style, juce::Slider&) override
    {
        using namespace juce;

        if (style == Slider::LinearVertical)
        {
            const float cx = (float) x + (float) w * 0.5f;
            const Rectangle<float> groove (cx - 3.0f, (float) y + 2.0f, 6.0f, (float) h - 4.0f);

            g.setColour (Colour (0xff070707));
            g.fillRoundedRectangle (groove.expanded (1.0f), 3.5f);

            auto filled = groove;
            filled = filled.withTop (sliderPos);
            g.setGradientFill (ColourGradient (theme::gold, 0.0f, filled.getY(),
                                               theme::goldDark, 0.0f, filled.getBottom(), false));
            g.fillRoundedRectangle (filled, 3.0f);

            // 0 dB tick marks
            g.setColour (Colour (0xff555555));
            for (int i = 0; i <= 6; ++i)
            {
                const float ty = groove.getY() + groove.getHeight() * (float) i / 6.0f;
                g.drawHorizontalLine ((int) ty, cx - 13.0f, cx - 8.0f);
            }

            const Rectangle<float> cap (cx - 15.0f, sliderPos - 9.0f, 30.0f, 18.0f);
            g.setColour (Colours::black.withAlpha (0.6f));
            g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 3.0f);
            g.setGradientFill (ColourGradient (Colour (0xff6a6a6a), 0.0f, cap.getY(),
                                               Colour (0xff262626), 0.0f, cap.getBottom(), false));
            g.fillRoundedRectangle (cap, 3.0f);
            g.setColour (Colour (0xff0a0a0a));
            g.drawRoundedRectangle (cap, 3.0f, 1.0f);
            g.setColour (theme::goldLight);
            g.fillRect (cap.getX() + 4.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 8.0f, 2.0f);
        }
        else
        {
            const float cy = (float) y + (float) h * 0.5f;
            const Rectangle<float> groove ((float) x + 2.0f, cy - 3.0f, (float) w - 4.0f, 6.0f);

            g.setColour (Colour (0xff070707));
            g.fillRoundedRectangle (groove.expanded (1.0f), 3.5f);

            auto filled = groove.withRight (sliderPos);
            g.setGradientFill (ColourGradient (theme::goldDark, filled.getX(), 0.0f,
                                               theme::gold, filled.getRight(), 0.0f, false));
            g.fillRoundedRectangle (filled, 3.0f);

            const Rectangle<float> cap (sliderPos - 9.0f, cy - 15.0f, 18.0f, 30.0f);
            g.setColour (Colours::black.withAlpha (0.6f));
            g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 3.0f);
            g.setGradientFill (ColourGradient (Colour (0xff6a6a6a), cap.getX(), 0.0f,
                                               Colour (0xff262626), cap.getRight(), 0.0f, false));
            g.fillRoundedRectangle (cap, 3.0f);
            g.setColour (Colour (0xff0a0a0a));
            g.drawRoundedRectangle (cap, 3.0f, 1.0f);
            g.setColour (theme::goldLight);
            g.fillRect (cap.getCentreX() - 1.0f, cap.getY() + 4.0f, 2.0f, cap.getHeight() - 8.0f);
        }
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto base = backgroundColour;

        if (down)             base = base.darker (0.25f);
        else if (highlighted) base = base.brighter (0.12f);

        g.setGradientFill (ColourGradient (base.brighter (0.18f), 0.0f, r.getY(),
                                           base.darker (0.3f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);

        g.setColour (theme::outline);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 4.0f, r.getRight() - 4.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (13.0f, (float) buttonHeight * 0.55f), juce::Font::bold));
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool isDown, int, int, int, int,
                       juce::ComboBox& box) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        const auto base = isDown ? theme::control.darker (0.2f) : theme::control;

        g.setGradientFill (ColourGradient (base.brighter (0.15f), 0.0f, r.getY(),
                                           base.darker (0.3f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::outline);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        Path arrow;
        const bool tiny = height < 20;
        const float ax = (float) width - (tiny ? 7.0f : 12.0f), ay = (float) height * 0.5f, k = tiny ? 0.65f : 1.0f;
        arrow.addTriangle (ax - 4.0f * k, ay - 2.0f * k, ax + 4.0f * k, ay - 2.0f * k, ax, ay + 3.0f * k);
        g.setColour (box.findColour (ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions (11.5f));
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return juce::Font (juce::FontOptions (box.getHeight() < 20 ? 10.0f : 13.0f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        if (box.getHeight() < 20) label.setBounds (3, 0, box.getWidth() - 12, box.getHeight());
        else label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};
