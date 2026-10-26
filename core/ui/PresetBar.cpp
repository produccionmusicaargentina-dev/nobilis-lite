#include "PresetBar.h"

using namespace juce;

static const NobPalette& palOf (Component& c)
{
    if (auto* laf = dynamic_cast<NobLookAndFeel*> (&c.getLookAndFeel()))
        return laf->pal();
    static const NobPalette fallback = NobPalette::nox();
    return fallback;
}

static bool darkOf (Component& c)
{
    if (auto* laf = dynamic_cast<NobLookAndFeel*> (&c.getLookAndFeel()))
        return laf->isDark();
    return true;
}

// Recessed "display window" used by the name field and the dialog
static void drawRecess (Graphics& g, Rectangle<float> r, bool dark, float corner)
{
    g.setColour (dark ? Colour (0xff070707) : Colour (0xffe7e3da));
    g.fillRoundedRectangle (r, corner);
    ColourGradient inner (Colours::black.withAlpha (dark ? 0.6f : 0.22f), r.getX(), r.getY(),
                          Colours::transparentBlack, r.getX(), r.getY() + r.getHeight() * 0.4f, false);
    g.setGradientFill (inner);
    g.fillRoundedRectangle (r, corner);
    g.setColour (dark ? Colours::white.withAlpha (0.10f) : Colours::white.withAlpha (0.9f));
    g.drawLine (r.getX() + corner, r.getBottom() + 0.5f, r.getRight() - corner, r.getBottom() + 0.5f, 1.0f);
    g.setColour (Colours::black.withAlpha (dark ? 0.9f : 0.25f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

//==============================================================================
void BarButton::paintButton (Graphics& g, bool isOver, bool isDown)
{
    const auto& p = palOf (*this);
    const bool dark = darkOf (*this);
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = b.getHeight() * 0.18f;

    Path body;
    body.addRoundedRectangle (b, corner);
    if (! isDown)
        DropShadow (Colours::black.withAlpha (dark ? 0.7f : 0.3f), roundToInt (b.getHeight() * 0.2f),
                    { 0, roundToInt (b.getHeight() * 0.08f) }).drawForPath (g, body);

    const Colour top = dark ? Colour (0xff2c2c2c) : Colour (0xffffffff);
    const Colour bot = dark ? Colour (0xff141414) : Colour (0xffdedad2);
    ColourGradient grad (isDown ? bot : top, b.getX(), b.getY(), isDown ? top : bot, b.getX(), b.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (body);
    g.setColour (p.ink.withAlpha (isOver ? 0.4f : 0.2f));
    g.strokePath (body, PathStrokeType (1.0f));

    const auto c = b.getCentre().translated (0.0f, isDown ? 0.5f : 0.0f);
    const float s = b.getHeight() * 0.22f;
    g.setColour (isOver ? p.accent : p.ink);

    if (kind == Prev || kind == Next)
    {
        Path tri;
        const float d = kind == Prev ? -1.0f : 1.0f;
        tri.addTriangle (c.x + d * s, c.y, c.x - d * s * 0.7f, c.y - s, c.x - d * s * 0.7f, c.y + s);
        g.fillPath (tri);
    }
    else if (kind == Undo || kind == Redo)
    {
        // curved arrow
        const float d = kind == Undo ? 1.0f : -1.0f;
        Path arc;
        arc.addCentredArc (c.x, c.y + s * 0.35f, s * 1.0f, s * 0.9f, 0.0f,
                           -d * MathConstants<float>::halfPi * 1.05f, d * MathConstants<float>::halfPi * 0.75f, true);
        g.strokePath (arc, PathStrokeType (jmax (1.2f, s * 0.24f), PathStrokeType::curved, PathStrokeType::rounded));
        const auto tip = Point<float> (c.x - d * s * 1.0f, c.y + s * 0.35f);
        Path head;
        head.addTriangle (tip.x - s * 0.5f, tip.y - s * 0.1f, tip.x + s * 0.5f, tip.y - s * 0.1f, tip.x, tip.y + s * 0.6f);
        g.fillPath (head);
    }
    else if (kind == Copy)
    {
        g.setFont (NobLookAndFeel::font (b.getHeight() * 0.36f, true, 0.18f));
        g.drawText ("COPY", b, Justification::centred);
    }
    else
    {
        // floppy-style save glyph
        auto r = Rectangle<float> (s * 2.2f, s * 2.2f).withCentre (c);
        Path f;
        f.startNewSubPath (r.getX(), r.getY());
        f.lineTo (r.getRight() - s * 0.5f, r.getY());
        f.lineTo (r.getRight(), r.getY() + s * 0.5f);
        f.lineTo (r.getRight(), r.getBottom());
        f.lineTo (r.getX(), r.getBottom());
        f.closeSubPath();
        g.strokePath (f, PathStrokeType (jmax (1.0f, s * 0.22f)));
        g.fillRect (r.getX() + s * 0.45f, r.getY(), r.getWidth() * 0.45f, s * 0.6f);
        g.fillRect (r.getX() + s * 0.45f, r.getBottom() - s * 0.8f, r.getWidth() - s * 0.9f, s * 0.8f);
    }
}

//==============================================================================
PresetBar::PresetBar (PresetManager& m) : pm (m)
{
    addAndMakeVisible (prev);
    addAndMakeVisible (next);
    addAndMakeVisible (save);

    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    save.setTooltip ("Save preset");

    prev.onClick = [this] { pm.loadPrevious(); refresh(); };
    next.onClick = [this] { pm.loadNext(); refresh(); };
    save.onClick = [this] { saveCurrent(); };
    refresh();
}

void PresetBar::saveCurrent()
{
    const int i = pm.getCurrentIndex();
    const bool isUser = i >= 0 && ! pm.getPresets()[(size_t) i].isFactory;
    if (isUser)
    {
        pm.saveUserPreset (pm.getCurrentName()); // overwrite
        refresh();
    }
    else if (onSaveAs)
    {
        onSaveAs();
    }
}

void PresetBar::refresh()
{
    const auto name = pm.getCurrentName();
    const bool mod = pm.isModified();
    if (name != shownName || mod != shownModified)
    {
        shownName = name;
        shownModified = mod;
        repaint();
    }
}

void PresetBar::resized()
{
    auto b = getLocalBounds();
    const int h = b.getHeight();
    prev.setBounds (b.removeFromLeft (h));
    b.removeFromLeft (h / 6);
    next.setBounds (b.removeFromLeft (h));
    save.setBounds (b.removeFromRight (h));
    b.removeFromRight (h / 4);
    b.removeFromLeft (h / 4);
    nameArea = b.toFloat();
}

void PresetBar::paint (Graphics& g)
{
    const auto& p = palOf (*this);
    const bool dark = darkOf (*this);
    const float h = nameArea.getHeight();

    drawRecess (g, nameArea, dark, h * 0.16f);

    // name, in an "LCD" warm tone on dark; ink on light
    const Colour txt = dark ? p.accent.brighter (0.15f) : p.ink;
    g.setColour (hoverName ? txt.brighter (0.3f) : txt);
    g.setFont (NobLookAndFeel::font (h * 0.50f, true, 0.06f));
    auto textArea = nameArea.reduced (h * 0.5f, 0.0f);
    g.drawText (shownName + (shownModified ? " *" : ""), textArea, Justification::centred, true);

    // dropdown chevron
    const float s = h * 0.13f;
    const Point<float> c (nameArea.getRight() - h * 0.45f, nameArea.getCentreY());
    Path chev;
    chev.startNewSubPath (c.x - s, c.y - s * 0.5f);
    chev.lineTo (c.x, c.y + s * 0.5f);
    chev.lineTo (c.x + s, c.y - s * 0.5f);
    g.setColour (txt.withAlpha (0.7f));
    g.strokePath (chev, PathStrokeType (jmax (1.0f, h * 0.06f), PathStrokeType::curved, PathStrokeType::rounded));
}

void PresetBar::mouseMove (const MouseEvent& e)
{
    const bool h = nameArea.contains (e.position);
    if (h != hoverName) { hoverName = h; repaint(); }
    setMouseCursor (h ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void PresetBar::mouseExit (const MouseEvent&)
{
    if (hoverName) { hoverName = false; repaint(); }
}

void PresetBar::mouseUp (const MouseEvent& e)
{
    if (nameArea.contains (e.position) && onBrowse)
        onBrowse();
}

//==============================================================================
SaveOverlay::SaveOverlay()
{
    setVisible (false);
    setWantsKeyboardFocus (false);

    nameEditor.setJustification (Justification::centred);
    nameEditor.setInputRestrictions (48);
    nameEditor.onReturnKey = [this] { confirm(); };
    nameEditor.onEscapeKey = [this] { close(); };
    nameEditor.onTextChange = [this] { repaint(); };
    addAndMakeVisible (nameEditor);

    addAndMakeVisible (ok);
    addAndMakeVisible (cancel);
    ok.onClick = [this] { confirm(); };
    cancel.onClick = [this] { close(); };
}

void SaveOverlay::show (const String& initialName)
{
    nameEditor.setText (initialName, false);
    setVisible (true);
    toFront (true);
    nameEditor.grabKeyboardFocus();
    nameEditor.selectAll();
    repaint();
}

void SaveOverlay::confirm()
{
    const auto name = nameEditor.getText().trim();
    if (name.isEmpty())
        return;
    close();
    if (onConfirm)
        onConfirm (name);
}

void SaveOverlay::close()
{
    setVisible (false);
}

void SaveOverlay::mouseDown (const MouseEvent& e)
{
    if (! card().contains (e.position))
        close();
}

Rectangle<float> SaveOverlay::card() const
{
    const float w = (float) getWidth() * 0.42f;
    return Rectangle<float> (w, w * 0.44f).withCentre (getLocalBounds().toFloat().getCentre());
}

void SaveOverlay::resized()
{
    const auto c = card();
    const float u = c.getHeight() / 10.0f;
    nameEditor.setBounds (c.withTrimmedTop (u * 3.0f).withHeight (u * 2.2f).reduced (u * 1.2f, 0.0f).toNearestInt());
    nameEditor.setFont (NobLookAndFeel::font (u * 1.1f, true, 0.04f));
    nameEditor.applyFontToAllText (NobLookAndFeel::font (u * 1.1f, true, 0.04f));

    auto row = c.withTrimmedTop (u * 6.6f).withHeight (u * 2.0f).reduced (u * 1.2f, 0.0f);
    const float bw = (row.getWidth() - u) * 0.5f;
    cancel.setBounds (row.removeFromLeft (bw).toNearestInt());
    row.removeFromLeft (u);
    ok.setBounds (row.toNearestInt());
}

void SaveOverlay::paint (Graphics& g)
{
    const auto& p = palOf (*this);
    const bool dark = darkOf (*this);
    g.fillAll (Colours::black.withAlpha (0.55f));

    const auto c = card();
    const float u = c.getHeight() / 10.0f;
    Path cp;
    cp.addRoundedRectangle (c, u * 0.6f);
    DropShadow (Colours::black.withAlpha (0.8f), roundToInt (u * 2), { 0, roundToInt (u * 0.6f) }).drawForPath (g, cp);
    g.setColour (p.panel);
    g.fillPath (cp);
    g.setColour (p.ink.withAlpha (0.3f));
    g.strokePath (cp, PathStrokeType (1.0f));

    g.setColour (p.ink);
    g.setFont (NobLookAndFeel::font (u * 1.0f, true, 0.4f));
    g.drawText ("SAVE PRESET", c.withHeight (u * 2.6f).translated (0, u * 0.4f), Justification::centred);

    if (existsCheck && existsCheck (nameEditor.getText().trim()))
    {
        g.setColour (p.accent);
        g.setFont (NobLookAndFeel::font (u * 0.7f, true, 0.1f));
        g.drawText ("A user preset with this name will be replaced",
                    c.withTrimmedTop (u * 5.3f).withHeight (u * 1.1f), Justification::centred);
    }
    ignoreUnused (dark);
}

void SaveOverlay::DialogButton::paintButton (Graphics& g, bool isOver, bool isDown)
{
    const auto& p = palOf (*this);
    const bool dark = darkOf (*this);
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = b.getHeight() * 0.16f;

    Colour fill = isPrimary ? p.accent : (dark ? Colour (0xff262626) : Colour (0xffffffff));
    if (isOver) fill = fill.brighter (0.1f);
    if (isDown) fill = fill.darker (0.15f);
    g.setColour (fill);
    g.fillRoundedRectangle (b, corner);
    g.setColour (p.ink.withAlpha (isPrimary ? 0.0f : 0.25f));
    g.drawRoundedRectangle (b, corner, 1.0f);

    g.setColour (isPrimary ? Colours::white : p.ink);
    g.setFont (NobLookAndFeel::font (b.getHeight() * 0.38f, true, 0.25f));
    g.drawText (getName(), b, Justification::centred);
}

//==============================================================================
void ABSwitch::paintButton (Graphics& g, bool isOver, bool)
{
    const auto& p = palOf (*this);
    const bool dark = darkOf (*this);
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = b.getHeight() * 0.18f;

    g.setColour (dark ? Colour (0xff0a0a0a) : Colour (0xffdcd8d0));
    g.fillRoundedRectangle (b, corner);
    g.setColour (p.ink.withAlpha (isOver ? 0.35f : 0.18f));
    g.drawRoundedRectangle (b, corner, 1.0f);

    auto left = b.withWidth (b.getWidth() * 0.5f), right = left.translated (left.getWidth(), 0.0f);
    auto active = onB ? right : left;
    const auto lit = p.glow (0.8f);
    g.setColour (lit.withAlpha (dark ? 0.9f : 0.85f));
    g.fillRoundedRectangle (active.reduced (b.getHeight() * 0.1f), corner * 0.7f);

    g.setFont (NobLookAndFeel::font (b.getHeight() * 0.42f, true, 0.0f));
    g.setColour (onB ? p.inkDim : Colours::white);
    g.drawText ("A", left, Justification::centred);
    g.setColour (onB ? Colours::white : p.inkDim);
    g.drawText ("B", right, Justification::centred);
}
