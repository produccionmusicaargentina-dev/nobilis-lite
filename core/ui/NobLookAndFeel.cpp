#include "NobLookAndFeel.h"
#include "Diagnostics.h"

using namespace juce;

static const NobPalette& paletteOf (Component& c)
{
    if (auto* laf = dynamic_cast<NobLookAndFeel*> (&c.getLookAndFeel()))
        return laf->pal();
    static const NobPalette fallback = NobPalette::nox();
    return fallback;
}

static bool isDarkTheme (Component& c)
{
    if (auto* laf = dynamic_cast<NobLookAndFeel*> (&c.getLookAndFeel()))
        return laf->isDark();
    return true;
}

//==============================================================================
NobLookAndFeel::NobLookAndFeel()
{
    setDark (true);
}

void NobLookAndFeel::setDark (bool shouldBeDark)
{
    dark = shouldBeDark;
    palette = dark ? NobPalette::nox() : NobPalette::lux();

    const auto menuBg = dark ? Colour (0xff161616) : Colour (0xfffaf8f3);
    setColour (PopupMenu::backgroundColourId, menuBg);
    setColour (PopupMenu::textColourId, palette.ink);
    setColour (PopupMenu::headerTextColourId, palette.inkDim);
    setColour (PopupMenu::highlightedBackgroundColourId, palette.accent);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);

    setColour (TextEditor::backgroundColourId, dark ? Colour (0xff0a0a0a) : Colour (0xffeae6dd));
    setColour (TextEditor::textColourId, palette.ink);
    setColour (TextEditor::highlightColourId, palette.accent.withAlpha (0.4f));
    setColour (TextEditor::highlightedTextColourId, palette.ink);
    setColour (TextEditor::outlineColourId, palette.ink.withAlpha (0.25f));
    setColour (TextEditor::focusedOutlineColourId, palette.accent);
    setColour (CaretComponent::caretColourId, palette.accent);
}

void NobLookAndFeel::drawPopupMenuBackground (Graphics& g, int width, int height)
{
    g.fillAll (findColour (PopupMenu::backgroundColourId));
    g.setColour (palette.ink.withAlpha (0.2f));
    g.drawRect (0, 0, width, height, 1);
}

Font NobLookAndFeel::font (float height, bool bold, float kerning)
{
    return Font (FontOptions (height, bold ? Font::bold : Font::plain)).withExtraKerningFactor (kerning);
}

void NobLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                         float pos, float start, float end, Slider& s)
{
    KnobStyle st;
    st.bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    st.ring    = (bool) s.getProperties().getWithDefault ("ring", true);
    st.enabled = s.isEnabled();
    st.hover   = s.isMouseOverOrDragging();
    drawKnob (g, Rectangle<int> (x, y, w, h).toFloat(), pos, start, end, st);
}

//==============================================================================
// The ACIES knob: a fluted (scalloped) black bakelite cap that turns with the value,
// a soft value ring around it and a cream pointer line. Matte, a little sheen only.
void NobLookAndFeel::drawKnob (Graphics& g, Rectangle<float> area, float pos,
                                 float start, float end, const KnobStyle& st)
{
    NOB_STEP_ONCE ("Knob: first paint");
    if (area.getWidth() < 4.0f || area.getHeight() < 4.0f)
        return;
    const auto c = area.getCentre();
    const float outer = jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const float R = outer / (st.ring ? 1.30f : 1.08f);
    const float angle = start + pos * (end - start);
    const bool big = R > 30.0f;
    auto pointAt = [c] (float radius, float a) { return c.getPointOnCircumference (radius, a); };

    // --- scale ticks ----------------------------------------------------------
    if (st.ring)
    {
        const int nTicks = 11;
        for (int i = 0; i < nTicks; ++i)
        {
            const float a = start + (end - start) * (float) i / (float) (nTicks - 1);
            const bool major = i == 0 || i == nTicks - 1 || i == nTicks / 2;
            g.setColour (palette.ink.withAlpha (major ? 0.55f : 0.28f));
            g.drawLine (Line<float> (pointAt (R * 1.25f, a), pointAt (R * (major ? 1.34f : 1.30f), a)), jmax (1.0f, R * 0.03f));
        }
    }

    // --- value ring: a soft lit groove (no glare) ------------------------------------
    if (st.ring)
    {
        const float rr = R * 1.13f, th = jmax (2.0f, R * 0.06f);
        Path track;
        track.addCentredArc (c.x, c.y, rr, rr, 0.0f, start, end, true);
        g.setColour (dark ? Colour (0xff060606) : Colour (0xffd2cdc3));
        g.strokePath (track, PathStrokeType (th * 1.5f, PathStrokeType::curved, PathStrokeType::rounded));

        const float from = st.bipolar ? (start + end) * 0.5f : start;
        if (std::abs (angle - from) > 0.002f)
        {
            Path val;
            val.addCentredArc (c.x, c.y, rr, rr, 0.0f, jmin (from, angle), jmax (from, angle), true);
            const auto col = palette.glow (0.62f + 0.25f * pos).withMultipliedAlpha (st.enabled ? 1.0f : 0.35f);
            g.setColour (col.withAlpha (st.hover ? 0.20f : 0.12f));             // gentle bloom
            g.strokePath (val, PathStrokeType (th * 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (col.withAlpha (0.92f));
            g.strokePath (val, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }

    // --- fluted body -----------------------------------------------------------------
    const int lobes = 8;
    Path body;
    {
        const int steps = 160;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = MathConstants<float>::twoPi * (float) i / (float) steps;
            // rounded flutes, turning with the knob
            const float r = R * (0.935f + 0.065f * std::cos ((float) lobes * (t - angle)));
            const auto p = c.getPointOnCircumference (r, t);
            if (i == 0) body.startNewSubPath (p); else body.lineTo (p);
        }
        body.closeSubPath();
    }
    DropShadow (Colours::black.withAlpha (dark ? 0.75f : 0.40f), roundToInt (R * 0.32f), { 0, roundToInt (R * 0.12f) }).drawForPath (g, body);
    g.setGradientFill (ColourGradient (Colour (0xff383838), c.x, c.y - R, Colour (0xff060606), c.x, c.y + R, false));
    g.fillPath (body);

    // satin sheen from the top left (kept low: matte bakelite, not chrome)
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (body);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (st.hover ? 0.13f : 0.09f), c.x - R * 0.45f, c.y - R * 0.6f,
                                           Colours::transparentWhite, c.x + R * 0.2f, c.y + R * 0.1f, true));
        g.fillEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));
        // flutes catch light on their crests
        for (int i = 0; i < lobes; ++i)
        {
            const float a = angle + MathConstants<float>::twoPi * (float) i / (float) lobes;
            const float facing = 0.5f + 0.5f * -std::cos (a);                   // crests facing up are brighter
            g.setColour (Colours::white.withAlpha (0.015f + 0.06f * facing));
            g.drawLine (Line<float> (pointAt (R * 0.70f, a), pointAt (R * 0.97f, a)), jmax (1.0f, R * 0.10f));
        }
    }
    // rim: light on top, shade below
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.30f), c.x, c.y - R, Colours::black.withAlpha (0.6f), c.x, c.y + R, false));
    g.strokePath (body, PathStrokeType (jmax (1.0f, R * 0.028f)));

    // --- inner cap (big knobs) ---------------------------------------------------------
    if (big)
    {
        const float Rc = R * 0.60f;
        const auto cap = Rectangle<float> (Rc * 2.0f, Rc * 2.0f).withCentre (c);
        g.setGradientFill (ColourGradient (Colour (0xff0b0b0b), c.x, c.y - Rc, Colour (0xff262626), c.x, c.y + Rc, false));
        g.fillEllipse (cap);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawEllipse (cap, jmax (1.0f, R * 0.02f));
        Path lip;
        lip.addCentredArc (c.x, c.y, Rc - 0.6f, Rc - 0.6f, 0.0f, 2.1f, 4.18f, true);
        g.setColour (Colours::white.withAlpha (0.14f));
        g.strokePath (lip, PathStrokeType (jmax (0.8f, R * 0.018f)));
    }

    // --- pointer: cream line ------------------------------------------------------------
    {
        const float pw = jmax (1.6f, R * 0.075f);
        Path line;
        line.startNewSubPath (pointAt (big ? R * 0.30f : R * 0.12f, angle));
        line.lineTo (pointAt (R * 0.86f, angle));
        g.setColour (Colours::black.withAlpha (0.55f));
        g.strokePath (line, PathStrokeType (pw * 1.7f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (st.enabled ? Colour (0xfff2ecdc) : Colour (0xff8a877f));
        g.strokePath (line, PathStrokeType (pw, PathStrokeType::curved, PathStrokeType::rounded));
    }

    if (! st.enabled)
    {
        g.setColour ((dark ? Colours::black : Colour (0xfff6f4ef)).withAlpha (0.45f));
        g.fillEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));
    }
}

//==============================================================================
static void drawChromeWasher (Graphics& g, Point<float> c, float r)
{
    ColourGradient grad (Colour (0xfff2f2f0), c.x - r, c.y - r,
                         Colour (0xff6d6d6b), c.x + r, c.y + r, false);
    g.setGradientFill (grad);
    g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c), 0.8f);
}

void BatToggle::paintButton (Graphics& g, bool isOver, bool)
{
    const auto b = getLocalBounds().toFloat();
    const float s = jmin (b.getWidth(), b.getHeight() * 0.6f);
    const auto c = b.getCentre();
    const bool on = getToggleState();

    // washer + hex nut
    DropShadow (Colours::black.withAlpha (0.6f), roundToInt (s * 0.12f), { 0, roundToInt (s * 0.06f) })
        .drawForPath (g, [&] { Path p; p.addEllipse (Rectangle<float> (s * 0.7f, s * 0.7f).withCentre (c)); return p; }());
    drawChromeWasher (g, c, s * 0.35f);

    Path hex;
    hex.addPolygon (c, 6, s * 0.24f, MathConstants<float>::pi / 6.0f);
    ColourGradient hg (Colour (0xffe0e0dd), c.x, c.y - s * 0.24f, Colour (0xff595957), c.x, c.y + s * 0.24f, false);
    g.setGradientFill (hg);
    g.fillPath (hex);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.strokePath (hex, PathStrokeType (0.8f));

    // lever
    const float len = b.getHeight() * 0.40f;
    const float dir = on ? -1.0f : 1.0f;
    const Point<float> tip (c.x, c.y + dir * len);
    const float wBase = s * 0.12f, wTip = s * 0.17f;

    Path lever;
    lever.startNewSubPath (c.x - wBase, c.y);
    lever.lineTo (tip.x - wTip * 0.8f, tip.y);
    lever.lineTo (tip.x + wTip * 0.8f, tip.y);
    lever.lineTo (c.x + wBase, c.y);
    lever.closeSubPath();
    lever.addEllipse (Rectangle<float> (wTip * 2.2f, wTip * 2.2f).withCentre (tip));

    // shadow falls down-right
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillPath (lever, AffineTransform::translation (s * 0.06f, s * 0.10f));

    ColourGradient chrome (Colour (0xff5a5a58), c.x - wTip, c.y,
                           Colour (0xff5a5a58), c.x + wTip, c.y, false);
    chrome.addColour (0.35, Colour (0xfffbfbf8));
    chrome.addColour (0.55, Colour (0xffbdbdba));
    g.setGradientFill (chrome);
    g.fillPath (lever);
    g.setColour (Colours::black.withAlpha (isOver ? 0.7f : 0.5f));
    g.strokePath (lever, PathStrokeType (0.8f));

    // highlight on the ball tip
    g.setColour (Colours::white.withAlpha (0.8f));
    g.fillEllipse (Rectangle<float> (wTip * 0.7f, wTip * 0.55f).withCentre (tip.translated (-wTip * 0.35f, -wTip * 0.4f)));
}

//==============================================================================
void PowerButton::paintButton (Graphics& g, bool isOver, bool isDown)
{
    const auto& p = paletteOf (*this);
    const bool dark = isDarkTheme (*this);
    const bool active = ! getToggleState();
    auto b = getLocalBounds().toFloat().reduced (1.5f);
    const float d = jmin (b.getWidth(), b.getHeight());
    auto r = Rectangle<float> (d, d).withCentre (b.getCentre()).translated (0.0f, isDown ? 0.5f : 0.0f);
    const auto c = r.getCentre();

    Path body; body.addEllipse (r);
    DropShadow (Colours::black.withAlpha (dark ? 0.7f : 0.3f), roundToInt (d * 0.2f), { 0, roundToInt (d * 0.06f) }).drawForPath (g, body);
    g.setGradientFill (ColourGradient (dark ? Colour (0xff2c2c2c) : Colour (0xffffffff), c.x, r.getY(),
                                       dark ? Colour (0xff121212) : Colour (0xffdcd8d0), c.x, r.getBottom(), false));
    g.fillPath (body);
    g.setColour (p.ink.withAlpha (isOver ? 0.4f : 0.2f));
    g.strokePath (body, PathStrokeType (1.0f));

    const auto col = active ? p.glow (0.85f) : p.inkDim.withAlpha (0.8f);
    const float rr = d * 0.24f;
    Path icon;
    icon.addCentredArc (c.x, c.y + d * 0.02f, rr, rr, 0.0f, 0.75f, MathConstants<float>::twoPi - 0.75f, true);
    icon.startNewSubPath (c.x, c.y - rr * 1.25f);
    icon.lineTo (c.x, c.y - rr * 0.15f);
    if (active)
    {
        g.setColour (col.withAlpha (0.35f));
        g.strokePath (icon, PathStrokeType (d * 0.16f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (col);
    g.strokePath (icon, PathStrokeType (jmax (1.4f, d * 0.075f), PathStrokeType::curved, PathStrokeType::rounded));
}

//==============================================================================
void SlideSwitch::paintButton (Graphics& g, bool isOver, bool)
{
    const auto& p = paletteOf (*this);
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float h = b.getHeight();

    // recessed slot
    g.setColour (p.switchSlot);
    g.fillRoundedRectangle (b, h * 0.5f);
    ColourGradient inner (Colours::black.withAlpha (0.45f), b.getX(), b.getY(),
                          Colours::transparentBlack, b.getX(), b.getY() + h * 0.45f, false);
    g.setGradientFill (inner);
    g.fillRoundedRectangle (b, h * 0.5f);
    g.setColour (p.ink.withAlpha (0.25f));
    g.drawRoundedRectangle (b, h * 0.5f, 1.0f);

    // thumb
    const bool right = getToggleState();
    auto thumb = b.reduced (h * 0.12f);
    thumb = right ? thumb.removeFromRight (thumb.getWidth() * 0.5f)
                  : thumb.removeFromLeft  (thumb.getWidth() * 0.5f);

    Path tp;
    tp.addRoundedRectangle (thumb, thumb.getHeight() * 0.5f);
    DropShadow (Colours::black.withAlpha (0.55f), roundToInt (h * 0.15f), { 0, roundToInt (h * 0.08f) }).drawForPath (g, tp);

    ColourGradient metal (Colour (0xfff4f4f1), thumb.getX(), thumb.getY(),
                          Colour (0xff8a8a87), thumb.getX(), thumb.getBottom(), false);
    g.setGradientFill (metal);
    g.fillPath (tp);

    // grip ridges
    for (int i = -2; i <= 2; ++i)
    {
        const float x = thumb.getCentreX() + (float) i * thumb.getWidth() * 0.09f;
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawLine (x, thumb.getY() + thumb.getHeight() * 0.28f, x, thumb.getBottom() - thumb.getHeight() * 0.28f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.6f));
        g.drawLine (x + 1.0f, thumb.getY() + thumb.getHeight() * 0.28f, x + 1.0f, thumb.getBottom() - thumb.getHeight() * 0.28f, 0.8f);
    }

    g.setColour (Colours::black.withAlpha (isOver ? 0.6f : 0.4f));
    g.strokePath (tp, PathStrokeType (0.8f));
}

//==============================================================================
void StyleButton::paintButton (Graphics& g, bool isOver, bool isDown)
{
    // Backlit acrylic key: a physical bezel with a translucent face lit from below
    const auto& p = paletteOf (*this);
    const bool dark = isDarkTheme (*this);
    const bool on = getToggleState();

    auto b = getLocalBounds().toFloat();
    auto key = b.withTrimmedBottom (b.getHeight() * 0.16f).reduced (2.0f, 1.0f);
    const float corner = key.getHeight() * 0.14f;
    const auto lit = p.glow (0.78f);

    // light spill on the panel under an active key
    if (on)
    {
        // soft elliptical pool of light on the panel
        Graphics::ScopedSaveState ss (g);
        const Point<float> c (key.getCentreX(), key.getBottom());
        const float r = key.getWidth() * 0.62f;
        g.addTransform (AffineTransform::scale (1.0f, (b.getBottom() - key.getBottom()) * 1.6f / r, c.x, c.y));
        g.setGradientFill (ColourGradient (lit.withAlpha (0.55f), c.x, c.y, lit.withAlpha (0.0f), c.x + r, c.y, true));
        g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    }

    // bezel
    Path bezel;
    bezel.addRoundedRectangle (key, corner);
    DropShadow (Colours::black.withAlpha (dark ? 0.75f : 0.30f), roundToInt (key.getHeight() * 0.22f),
                { 0, roundToInt (key.getHeight() * (isDown ? 0.03f : 0.09f)) }).drawForPath (g, bezel);
    g.setColour (dark ? Colour (0xff0c0c0c) : Colour (0xff2a2a2a));
    g.fillPath (bezel);

    // acrylic face
    auto face = key.reduced (key.getHeight() * 0.07f).translated (0.0f, isDown ? 1.0f : 0.0f);
    const float fc = corner * 0.7f;
    Path facePath;
    facePath.addRoundedRectangle (face, fc);

    const Colour baseTop = dark ? Colour (0xff222428) : Colour (0xfff4f2ee);
    const Colour baseBot = dark ? Colour (0xff131417) : Colour (0xffd9d5ce);
    g.setGradientFill (ColourGradient (baseTop, face.getX(), face.getY(), baseBot, face.getX(), face.getBottom(), false));
    g.fillPath (facePath);

    if (on)
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (facePath);
        // light rising from the LED strip at the bottom edge
        ColourGradient rise (lit.withAlpha (dark ? 0.95f : 0.85f), face.getCentreX(), face.getBottom(),
                             lit.withAlpha (0.0f), face.getCentreX(), face.getY() + face.getHeight() * 0.15f, false);
        g.setGradientFill (rise);
        g.fillRect (face);
        // the LED strip itself
        g.setColour (p.glow (1.0f).brighter (0.2f));
        g.fillRect (face.withTop (face.getBottom() - jmax (1.5f, face.getHeight() * 0.06f)).reduced (face.getWidth() * 0.08f, 0.0f));
    }
    else if (isOver)
    {
        g.setColour (lit.withAlpha (0.08f));
        g.fillPath (facePath);
    }

    // glossy top reflection
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (facePath);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (dark ? 0.10f : 0.55f), face.getX(), face.getY(),
                                           Colours::transparentWhite, face.getX(), face.getY() + face.getHeight() * 0.45f, false));
        g.fillRect (face.withHeight (face.getHeight() * 0.45f));
    }
    g.setColour (Colours::white.withAlpha (dark ? 0.08f : 0.5f));
    g.strokePath (facePath, PathStrokeType (0.8f));

    // engraved, edge-lit label
    const bool small = face.getHeight() < 32.0f;
    const auto font = NobLookAndFeel::font (face.getHeight() * (small ? 0.46f : 0.34f), true, small ? 0.06f : 0.2f);
    g.setFont (font);
    if (on)
    {
        if (dark)
        {
            g.setColour (lit.withAlpha (0.6f));
            g.drawText (getName(), face.expanded (1.0f), Justification::centred);
            g.setColour (Colours::white.withAlpha (0.95f));
        }
        else
        {
            g.setColour (Colours::white.withAlpha (0.5f));            // engraved highlight
            g.drawText (getName(), face.translated (0.0f, 1.0f), Justification::centred);
            g.setColour (Colour (0xff4a1d06));                       // dark ember: readable on the lit acrylic
        }
    }
    else
    {
        g.setColour (dark ? Colours::black.withAlpha (0.6f) : Colours::white.withAlpha (0.8f));
        g.drawText (getName(), face.translated (0.0f, 1.0f), Justification::centred);
        g.setColour (dark ? p.inkDim : p.inkDim.darker (0.2f));
    }
    g.drawText (getName(), face, Justification::centred);
}

