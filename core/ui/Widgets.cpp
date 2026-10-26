#include "Widgets.h"

using namespace juce;

namespace nob::ui
{

const NobPalette& palOf (Component& c)
{
    if (auto* laf = dynamic_cast<NobLookAndFeel*> (&c.getLookAndFeel()))
        return laf->pal();
    static const NobPalette fallback = NobPalette::nox();
    return fallback;
}

bool darkOf (Component& c)
{
    if (auto* laf = dynamic_cast<NobLookAndFeel*> (&c.getLookAndFeel()))
        return laf->isDark();
    return true;
}

static void setupRotary (Slider& s)
{
    s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    s.setVelocityBasedMode (false);
    s.setMouseDragSensitivity (220);
    s.setDoubleClickReturnValue (true, 0.0);       // the attachment replaces it with the parameter default
    s.setPopupDisplayEnabled (true, true, nullptr, 1200);
    s.setScrollWheelEnabled (true);
}

static void resetOnDoubleClick (Slider& s, AudioProcessorValueTreeState& st, const String& id)
{
    if (auto* p = st.getParameter (id))
        s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

//==============================================================================
Knob::Knob (AudioProcessorValueTreeState& st, const String& id, const String& cap,
            const String& lo, const String& hi, bool bipolar)
    : caption (cap), minText (lo), maxText (hi)
{
    setupRotary (slider);
    slider.getProperties().set ("bipolar", bipolar);
    addAndMakeVisible (slider);
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (st, id, slider);
    resetOnDoubleClick (slider, st, id);
    if (auto* p = st.getParameter (id)) slider.setTooltip (p->getName (64));
}

void Knob::resized()
{
    auto b = getLocalBounds().toFloat();
    b.removeFromTop (caption.isNotEmpty() ? 18.0f : 0.0f);
    b.removeFromBottom (minText.isNotEmpty() || maxText.isNotEmpty() ? 14.0f : 2.0f);
    const float s = jmin (b.getWidth(), b.getHeight());
    knobArea = b.withSizeKeepingCentre (s, s);
    slider.setBounds (knobArea.toNearestInt());
}

void Knob::paint (Graphics& g)
{
    const auto& p = palOf (*this);
    if (caption.isNotEmpty())
    {
        g.setColour (p.ink);
        g.setFont (NobLookAndFeel::font (10.5f, true, 0.30f));
        g.drawText (caption, getLocalBounds().removeFromTop (16), Justification::centred);
    }
    if (minText.isNotEmpty() || maxText.isNotEmpty())
    {
        g.setColour (p.inkDim);
        g.setFont (NobLookAndFeel::font (8.0f, true, 0.12f));
        const float y = (float) getHeight() - 13.0f;
        const float x0 = jmax (0.0f, knobArea.getX() - 10.0f), x1 = jmin ((float) getWidth(), knobArea.getRight() + 10.0f);
        const float half = (x1 - x0) * 0.5f;
        g.drawText (minText, Rectangle<float> (x0, y, half, 12.0f), Justification::centredLeft);
        g.drawText (maxText, Rectangle<float> (x0 + half, y, half, 12.0f), Justification::centredRight);
    }
}

//==============================================================================
HeroKnob::HeroKnob (AudioProcessorValueTreeState& st, const String& id, const String& cap) : caption (cap)
{
    setupRotary (slider);
    slider.getProperties().set ("ring", false);
    addAndMakeVisible (slider);
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (st, id, slider);
    resetOnDoubleClick (slider, st, id);
    slider.onValueChange = [this] { repaint(); };
    if (auto* p = st.getParameter (id)) slider.setTooltip (p->getName (64));
    startTimerHz (30);
}

void HeroKnob::timerCallback()
{
    if (! activity) return;
    const float a = jlimit (0.0f, 1.0f, activity());
    if (std::abs (a - shownActivity) > 0.004f)
    {
        shownActivity = a;
        repaint();
    }
}

void HeroKnob::resized()
{
    auto b = getLocalBounds().toFloat();
    b.removeFromBottom (caption.isNotEmpty() ? 26.0f : 0.0f);
    const float s = jmin (b.getWidth(), b.getHeight());
    const auto ring = b.withSizeKeepingCentre (s, s);
    knobArea = ring.reduced (s * 0.20f);                 // room for LEDs + numerals
    slider.setBounds (knobArea.toNearestInt());
}

void HeroKnob::paint (Graphics& g)
{
    const auto& p = palOf (*this);
    const bool dark = darkOf (*this);
    const auto rp = slider.getRotaryParameters();
    const float start = rp.startAngleRadians, end = rp.endAngleRadians;
    const float pos = (float) slider.valueToProportionOfLength (slider.getValue());
    const auto c = knobArea.getCentre();
    const float R = knobArea.getWidth() * 0.5f / 1.08f;     // body radius used by drawKnob (ring = false)

    // LED segments
    const int n = 41;
    const float r0 = R * 1.17f, r1 = R * 1.33f;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) (n - 1);
        const float a = start + t * (end - start);
        const bool lit = t <= pos + 1.0e-4f && pos > 0.001f;
        Line<float> seg (c.getPointOnCircumference (r0, a), c.getPointOnCircumference (r1, a));
        if (lit)
        {
            auto col = p.glow (0.45f + 0.55f * t);
            col = col.interpolatedWith (p.glow (1.0f), shownActivity * 0.6f);
            g.setColour (col.withAlpha (0.22f + 0.2f * shownActivity));
            g.drawLine (seg, R * 0.085f);
            g.setColour (col);
            g.drawLine (seg, R * 0.035f);
        }
        else
        {
            g.setColour (dark ? Colour (0xff050505) : Colour (0xffcfcac0));
            g.drawLine (seg, R * 0.04f);
        }
    }

    // numerals (or names)
    g.setColour (p.ink.withAlpha (0.85f));
    if (scaleNames.isEmpty())
    {
        g.setFont (NobLookAndFeel::font (jmax (9.0f, R * 0.17f), true, 0.0f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = start + (float) i / 10.0f * (end - start);
            const auto q = c.getPointOnCircumference (R * 1.52f, a);
            g.drawText (String (i), Rectangle<float> (R * 0.4f, R * 0.22f).withCentre (q), Justification::centred);
        }
    }
    else
    {
        g.setFont (NobLookAndFeel::font (jmax (8.0f, R * 0.12f), true, 0.15f));
        for (int i = 0; i < scaleNames.size(); ++i)
        {
            const float t = scaleNames.size() == 1 ? 0.5f : (float) i / (float) (scaleNames.size() - 1);
            const float a = start + t * (end - start);
            const auto q = c.getPointOnCircumference (R * 1.55f, a);
            g.drawText (scaleNames[i], Rectangle<float> (R * 1.2f, R * 0.22f).withCentre (q), Justification::centred);
        }
    }

    if (caption.isNotEmpty())
    {
        g.setColour (p.ink);
        g.setFont (NobLookAndFeel::font (13.0f, true, 0.42f));
        const float y = jmin ((float) getHeight() - 22.0f, c.y + R * 1.62f);
        g.drawText (caption, Rectangle<float> (0.0f, y, (float) getWidth(), 22.0f), Justification::centred);
    }
}

//==============================================================================
ChoiceKeys::ChoiceKeys (AudioProcessorValueTreeState& st, const String& id, const StringArray& labels, int cols)
    : columns (cols)
{
    param = st.getParameter (id);
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* k = keys.add (new StyleButton (labels[i]));
        k->setClickingTogglesState (false);
        k->onClick = [this, i]
        {
            if (att != nullptr) att->setValueAsCompleteGesture ((float) i);
        };
        addAndMakeVisible (k);
    }
    if (param != nullptr)
        att = std::make_unique<ParameterAttachment> (*param, [this] (float v)
        {
            current = roundToInt (v);
            for (int i = 0; i < keys.size(); ++i)
                keys[i]->setToggleState (i == current, dontSendNotification);
        }, st.undoManager);
    if (att != nullptr) att->sendInitialUpdate();
}

ChoiceKeys::~ChoiceKeys() = default;

void ChoiceKeys::resized()
{
    const int n = keys.size();
    if (n == 0) return;
    const int cols = columns > 0 ? columns : n;
    const int rows = (n + cols - 1) / cols;
    const auto b = getLocalBounds().toFloat();
    const float w = (b.getWidth() - gap * (float) (cols - 1)) / (float) cols;
    const float h = (b.getHeight() - gap * (float) (rows - 1)) / (float) rows;
    for (int i = 0; i < n; ++i)
    {
        const int r = i / cols, c = i % cols;
        keys[i]->setBounds (Rectangle<float> (b.getX() + (float) c * (w + gap), b.getY() + (float) r * (h + gap), w, h).toNearestInt());
    }
}

//==============================================================================
ToggleKey::ToggleKey (AudioProcessorValueTreeState& st, const String& id, const String& label) : StyleButton (label)
{
    setClickingTogglesState (true);
    att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (st, id, *this);
}

//==============================================================================
Toggle::Toggle (AudioProcessorValueTreeState& st, const String& id, const String& cap) : bat (cap), caption (cap)
{
    addAndMakeVisible (bat);
    att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (st, id, bat);
    if (auto* p = st.getParameter (id)) bat.setTooltip (p->getName (64));
}

void Toggle::resized()
{
    bat.setBounds (getLocalBounds().withTrimmedBottom (16));
}

void Toggle::paint (Graphics& g)
{
    const auto& p = palOf (*this);
    g.setColour (bat.getToggleState() ? p.ink : p.inkDim);
    g.setFont (NobLookAndFeel::font (8.5f, true, 0.25f));
    g.drawText (caption, getLocalBounds().removeFromBottom (14), Justification::centred);
}

//==============================================================================
void GlassScreen::paint (Graphics& g)
{
    const auto& p = palOf (*this);
    const bool dark = darkOf (*this);
    auto b = getLocalBounds().toFloat();

    // bezel
    Path bezel;
    bezel.addRoundedRectangle (b, 10.0f);
    g.setColour (dark ? Colour (0xff050505) : Colour (0xff1c1b1a));
    g.fillPath (bezel);
    auto glass = b.reduced (5.0f);
    Path gp;
    gp.addRoundedRectangle (glass, 6.0f);
    g.setColour (p.screen);
    g.fillPath (gp);

    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (gp);
        // faint grid
        g.setColour (p.glow1.withAlpha (0.05f));
        for (float x = glass.getX() + 16.0f; x < glass.getRight(); x += 16.0f) g.drawVerticalLine ((int) x, glass.getY(), glass.getBottom());
        for (float y = glass.getY() + 16.0f; y < glass.getBottom(); y += 16.0f) g.drawHorizontalLine ((int) y, glass.getX(), glass.getRight());

        paintScreen (g, glass, p);

        // labels
        g.setFont (NobLookAndFeel::font (8.5f, true, 0.3f));
        g.setColour (p.screenDim);
        if (label.isNotEmpty())      g.drawText (label, glass.reduced (10.0f, 7.0f).removeFromTop (12.0f), Justification::centredLeft);
        if (rightLabel.isNotEmpty())
        {
            g.setColour (p.glow (0.8f));
            g.drawText (rightLabel, glass.reduced (10.0f, 7.0f).removeFromTop (12.0f), Justification::centredRight);
        }

        // glass reflection
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.06f), glass.getX(), glass.getY(),
                                           Colours::transparentWhite, glass.getX(), glass.getY() + glass.getHeight() * 0.5f, false));
        g.fillRect (glass.withHeight (glass.getHeight() * 0.5f));
        // inner shadow
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.55f), glass.getX(), glass.getY(),
                                           Colours::transparentBlack, glass.getX(), glass.getY() + 10.0f, false));
        g.fillRect (glass.withHeight (10.0f));
    }
    g.setColour (Colours::white.withAlpha (dark ? 0.05f : 0.12f));
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);
}

//==============================================================================
void drawSectionTitle (Graphics& g, const NobPalette& p, Rectangle<float> area, const String& text, Justification just)
{
    const auto f = NobLookAndFeel::font (10.5f, true, 0.42f);
    g.setFont (f);
    const float tw = GlyphArrangement::getStringWidth (f, text);
    float x = area.getX();
    if (just.testFlags (Justification::horizontallyCentred)) x = area.getCentreX() - (tw + 26.0f) * 0.5f;
    g.setColour (p.accent);
    g.fillRect (Rectangle<float> (x, area.getCentreY() - 1.0f, 16.0f, 2.0f));
    g.setColour (p.ink);
    g.drawText (text, Rectangle<float> (x + 24.0f, area.getY(), tw + 10.0f, area.getHeight()), Justification::centredLeft);
}

void drawPlate (Graphics& g, const NobPalette& p, bool dark, Rectangle<float> area)
{
    g.setColour (dark ? Colours::black.withAlpha (0.22f) : Colours::black.withAlpha (0.035f));
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (p.panelEdge);
    g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);
}

void drawLamp (Graphics& g, Point<float> c, float r, Colour col, float level)
{
    level = jlimit (0.0f, 1.0f, level);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (Rectangle<float> (r * 2.6f, r * 2.6f).withCentre (c));
    if (level > 0.01f)
    {
        g.setGradientFill (ColourGradient (col.withAlpha (0.45f * level), c.x, c.y, col.withAlpha (0.0f), c.x + r * 3.2f, c.y, true));
        g.fillEllipse (Rectangle<float> (r * 6.4f, r * 6.4f).withCentre (c));
    }
    g.setGradientFill (ColourGradient (col.interpolatedWith (Colours::white, 0.5f * level).withMultipliedBrightness (0.35f + 0.65f * level),
                                       c.x - r * 0.3f, c.y - r * 0.3f,
                                       col.withMultipliedBrightness (0.25f + 0.6f * level), c.x + r, c.y + r, true));
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (Colours::white.withAlpha (0.35f));
    g.fillEllipse (Rectangle<float> (r * 0.7f, r * 0.5f).withCentre (c.translated (-r * 0.3f, -r * 0.4f)));
}

} // namespace nob::ui
