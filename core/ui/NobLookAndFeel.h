#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Colour palette for the two themes (LUX = white, NOX = black)
struct NobPalette
{
    juce::Colour tolex, tolexSpeck;
    juce::Colour panel, panelEdge, piping;
    juce::Colour ink, inkDim;
    juce::Colour meterFace, meterInk;
    juce::Colour switchSlot;
    juce::Colour accent;                 // ember (LUX) / electric blue (NOX)
    juce::Colour glow0, glow1, glow2;    // cold -> mid -> hot light colour
    // screens (analyzer glass): always dark, lit in the theme colour
    juce::Colour screen, screenEdge, screenInk, screenDim, curve;

    // Light colour for a "heat" 0..1 (LEDs, tube, backlights)
    juce::Colour glow (float t) const
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        return t < 0.5f ? glow0.interpolatedWith (glow1, t * 2.0f)
                        : glow1.interpolatedWith (glow2, (t - 0.5f) * 2.0f);
    }

    static NobPalette lux()
    {
        NobPalette p;
        p.tolex      = juce::Colour (0xffe6e2d9);
        p.tolexSpeck = juce::Colour (0xffc9c4b9);
        p.panel      = juce::Colour (0xfff6f4ef);
        p.panelEdge  = juce::Colour (0xffd8d3c9);
        p.piping     = juce::Colour (0xff1a1a1a);
        p.ink        = juce::Colour (0xff141414);
        p.inkDim     = juce::Colour (0xff8d897f);
        p.meterFace  = juce::Colour (0xfffcfbf7);
        p.meterInk   = juce::Colour (0xff141414);
        p.switchSlot = juce::Colour (0xffcfcac0);
        p.accent     = juce::Colour (0xffff5a1f);
        p.glow0      = juce::Colour (0xffa01800);
        p.glow1      = juce::Colour (0xffff4d10);
        p.glow2      = juce::Colour (0xffffb040);
        p.screen     = juce::Colour (0xff100f0e);
        p.screenEdge = juce::Colour (0xff2a2724);
        p.screenInk  = juce::Colour (0xffece6dc);
        p.screenDim  = juce::Colour (0xff8f877c);
        p.curve      = juce::Colour (0xffffe2cf);
        return p;
    }

    static NobPalette nox()
    {
        NobPalette p;
        p.tolex      = juce::Colour (0xff0c0c0c);
        p.tolexSpeck = juce::Colour (0xff232323);
        p.panel      = juce::Colour (0xff1b1b1b);
        p.panelEdge  = juce::Colour (0xff2e2e2e);
        p.piping     = juce::Colour (0xffe9e6df);
        p.ink        = juce::Colour (0xffece9e2);
        p.inkDim     = juce::Colour (0xff7d7a73);
        p.meterFace  = juce::Colour (0xff0b0b0b);
        p.meterInk   = juce::Colour (0xffe9e5da);
        p.switchSlot = juce::Colour (0xff0a0a0a);
        p.accent     = juce::Colour (0xff2f8cff);
        p.glow0      = juce::Colour (0xff0b2a8c);
        p.glow1      = juce::Colour (0xff1f7bff);
        p.glow2      = juce::Colour (0xffa8ecff);
        p.screen     = juce::Colour (0xff0a0a0b);
        p.screenEdge = juce::Colour (0xff262626);
        p.screenInk  = juce::Colour (0xffe9ebee);
        p.screenDim  = juce::Colour (0xff8d8a83);
        p.curve      = juce::Colour (0xffd2e6ff);
        return p;
    }
};

//==============================================================================
class NobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NobLookAndFeel();

    void setDark (bool shouldBeDark);
    bool isDark() const noexcept { return dark; }
    const NobPalette& pal() const noexcept { return palette; }

    // Font helper: tracked, bold sans
    static juce::Font font (float height, bool bold = true, float kerning = 0.12f);

    float menuScale = 1.0f;
    juce::Font getPopupMenuFont() override { return font (14.0f * menuScale, true, 0.04f); }
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    struct KnobStyle { bool ring = true, bipolar = false, enabled = true, hover = false; };
    // The fluted knob, reusable (the GAIN knob draws its dynamic ring on top).
    void drawKnob (juce::Graphics&, juce::Rectangle<float> area, float proportion,
                   float startAngle, float endAngle, const KnobStyle&);

private:
    bool dark = true;
    NobPalette palette = NobPalette::nox();
};

//==============================================================================
// Chrome "bat" toggle switch, like on a guitar amp.
class BatToggle : public juce::Button
{
public:
    explicit BatToggle (const juce::String& name) : juce::Button (name)
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
};

//==============================================================================
// Horizontal slide switch (used for the LUX / NOX theme).
class PowerButton : public juce::Button
{
public:
    // toggle state ON = bypassed (so it can attach to the "bypass" parameter)
    PowerButton() : juce::Button ("power") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
};

//==============================================================================
class SlideSwitch : public juce::Button
{
public:
    SlideSwitch() : juce::Button ("theme") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
};

//==============================================================================
// Push button with a LED above it, for the style selector.
class StyleButton : public juce::Button
{
public:
    explicit StyleButton (const juce::String& name) : juce::Button (name) {}
    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
};

