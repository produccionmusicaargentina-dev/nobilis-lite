#include "PresetBrowser.h"

using namespace juce;

static const NobPalette& palB (Component& c)
{
    if (auto* laf = dynamic_cast<NobLookAndFeel*> (&c.getLookAndFeel()))
        return laf->pal();
    static const NobPalette fallback = NobPalette::nox();
    return fallback;
}

static bool darkB (Component& c)
{
    if (auto* laf = dynamic_cast<NobLookAndFeel*> (&c.getLookAndFeel()))
        return laf->isDark();
    return true;
}

static String prettyCategory (const String& c)
{
    // "02 Drums" -> "Drums"
    return c.length() > 3 && CharacterFunctions::isDigit (c[0]) && c[2] == ' ' ? c.substring (3) : c;
}

//==============================================================================
PresetBrowser::PresetBrowser (PresetManager& m) : pm (m)
{
    setVisible (false);
    setWantsKeyboardFocus (true);

    search.setTextToShowWhenEmpty ("Search presets...", Colours::grey);
    search.setJustification (Justification::centredLeft);
    search.onTextChange = [this] { scroll = 0.0f; rebuildList(); repaint(); };
    search.onEscapeKey  = [this] { close(); };
    search.onReturnKey  = [this]
    {
        if (! rows.empty()) { pm.load (rows.front()); if (onChanged) onChanged(); close(); }
    };
    addAndMakeVisible (search);

    for (auto* b : { &saveBtn, &saveAsBtn, &deleteBtn, &folderBtn })
        addAndMakeVisible (b);

    saveBtn.onClick = [this]
    {
        const int i = pm.getCurrentIndex();
        if (i >= 0 && ! pm.getPresets()[(size_t) i].isFactory)
        {
            pm.saveUserPreset (pm.getCurrentName());
            if (onChanged) onChanged();
        }
        else if (onSaveAs) { close(); onSaveAs(); }
    };
    saveAsBtn.onClick = [this] { close(); if (onSaveAs) onSaveAs(); };
    folderBtn.onClick = [this] { pm.getUserDirectory().revealToUser(); };
    deleteBtn.onClick = [this]
    {
        const int i = pm.getCurrentIndex();
        if (i < 0 || pm.getPresets()[(size_t) i].isFactory) return;

        const auto now = Time::getMillisecondCounter();
        if (deleteArmedAt != 0 && now - deleteArmedAt < 3000)
        {
            deleteArmedAt = 0;
            pm.deleteUserPreset (i);
            rebuildList();
            if (onChanged) onChanged();
        }
        else
        {
            deleteArmedAt = now;
            SafePointer<PresetBrowser> safe (this);
            Timer::callAfterDelay (3000, [safe] { if (safe != nullptr) { safe->deleteArmedAt = 0; safe->updateButtons(); } });
        }
        updateButtons();
    };
}

void PresetBrowser::open (Rectangle<float> bounds)
{
    panel = bounds;
    categories.clear();
    categories.add ("All");
    for (const auto& p : pm.getPresets())
        if (p.isFactory && p.category.isNotEmpty() && ! categories.contains (p.category))
            categories.add (p.category);
    categories.add ("User");

    // start on the current preset's category
    selectedCategory = 0;
    const int cur = pm.getCurrentIndex();
    if (cur >= 0)
    {
        const auto& p = pm.getPresets()[(size_t) cur];
        const int c = categories.indexOf (p.isFactory ? p.category : String ("User"));
        if (c > 0) selectedCategory = c;
    }

    search.clear();
    rebuildList();
    ensureCurrentVisible();
    resized();
    updateButtons();
    setVisible (true);
    toFront (true);
    search.grabKeyboardFocus();
}

void PresetBrowser::close()
{
    setVisible (false);
}

void PresetBrowser::rebuildList()
{
    rows.clear();
    const auto q = search.getText().trim();
    const auto& all = pm.getPresets();
    const auto cat = categories[selectedCategory];

    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& p = all[(size_t) i];
        bool inCat = cat == "All" || (cat == "User" ? ! p.isFactory : (p.isFactory && p.category == cat));
        if (q.isNotEmpty())
            inCat = p.name.containsIgnoreCase (q) || prettyCategory (p.category).containsIgnoreCase (q);  // search spans everything
        if (inCat)
            rows.push_back (i);
    }
}

float PresetBrowser::rowHeight() const     { return panel.getHeight() / 15.0f; }

Rectangle<float> PresetBrowser::categoryArea() const
{
    const float u = panel.getHeight() / 30.0f;
    return { panel.getX() + u, panel.getY() + 4.2f * u, panel.getWidth() * 0.28f, panel.getHeight() - 8.6f * u };
}

Rectangle<float> PresetBrowser::listArea() const
{
    const auto c = categoryArea();
    const float u = panel.getHeight() / 30.0f;
    return { c.getRight() + u, c.getY(), panel.getRight() - u - (c.getRight() + u), c.getHeight() };
}

void PresetBrowser::ensureCurrentVisible()
{
    const int cur = pm.getCurrentIndex();
    const auto it = std::find (rows.begin(), rows.end(), cur);
    if (it == rows.end()) { scroll = 0.0f; return; }
    const float y = (float) std::distance (rows.begin(), it) * rowHeight();
    const float h = listArea().getHeight();
    if (y < scroll) scroll = y;
    if (y + rowHeight() > scroll + h) scroll = y + rowHeight() - h;
}

void PresetBrowser::resized()
{
    if (panel.isEmpty()) return;
    const float u = panel.getHeight() / 30.0f;

    search.setBounds (Rectangle<float> (panel.getX() + u, panel.getY() + u, panel.getWidth() * 0.62f, 2.4f * u).toNearestInt());
    const auto f = NobLookAndFeel::font (u * 1.15f, true, 0.04f);
    search.setFont (f);
    search.applyFontToAllText (f);
    search.setIndents (roundToInt (u * 0.8f), roundToInt ((2.4f * u - f.getHeight()) * 0.5f));

    auto footer = Rectangle<float> (panel.getX() + u, panel.getBottom() - 3.4f * u, panel.getWidth() - 2.0f * u, 2.4f * u);
    const float bw = (footer.getWidth() - 3.0f * u) / 4.0f;
    for (auto* b : { &saveBtn, &saveAsBtn, &deleteBtn, &folderBtn })
    {
        b->setBounds (footer.removeFromLeft (bw).toNearestInt());
        footer.removeFromLeft (u);
    }
}

void PresetBrowser::updateButtons()
{
    const int i = pm.getCurrentIndex();
    const bool isUser = i >= 0 && ! pm.getPresets()[(size_t) i].isFactory;
    saveBtn.setEnabled (true);
    deleteBtn.setEnabled (isUser);
    deleteBtn.setButtonText (deleteArmedAt != 0 ? "CONFIRM?" : "DELETE");
    repaint();
}

int PresetBrowser::categoryAt (Point<float> p) const
{
    const auto a = categoryArea();
    if (! a.contains (p)) return -1;
    const int i = (int) ((p.y - a.getY()) / rowHeight());
    return isPositiveAndBelow (i, categories.size()) ? i : -1;
}

int PresetBrowser::rowAt (Point<float> p) const
{
    const auto a = listArea();
    if (! a.contains (p)) return -1;
    const int i = (int) ((p.y - a.getY() + scroll) / rowHeight());
    return isPositiveAndBelow (i, (int) rows.size()) ? i : -1;
}

//==============================================================================
void PresetBrowser::mouseMove (const MouseEvent& e)
{
    const int r = rowAt (e.position), c = categoryAt (e.position);
    if (r != hoverRow || c != hoverCategory) { hoverRow = r; hoverCategory = c; repaint(); }
}

void PresetBrowser::mouseExit (const MouseEvent&)
{
    hoverRow = hoverCategory = -1;
    repaint();
}

void PresetBrowser::mouseDown (const MouseEvent& e)
{
    if (! panel.contains (e.position)) { close(); return; }

    if (const int c = categoryAt (e.position); c >= 0)
    {
        selectedCategory = c;
        search.clear();
        scroll = 0.0f;
        rebuildList();
        repaint();
        return;
    }
    if (const int r = rowAt (e.position); r >= 0)
    {
        pm.load (rows[(size_t) r]);
        updateButtons();
        if (onChanged) onChanged();
        repaint();
    }
}

void PresetBrowser::mouseDoubleClick (const MouseEvent& e)
{
    if (rowAt (e.position) >= 0)
        close();
}

void PresetBrowser::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    const float maxScroll = jmax (0.0f, (float) rows.size() * rowHeight() - listArea().getHeight());
    scroll = jlimit (0.0f, maxScroll, scroll - w.deltaY * rowHeight() * 4.0f);
    repaint();
}

bool PresetBrowser::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::escapeKey) { close(); return true; }
    return false;
}

//==============================================================================
void PresetBrowser::paint (Graphics& g)
{
    const auto& p = palB (*this);
    const bool dark = darkB (*this);
    const float u = panel.getHeight() / 30.0f;

    g.fillAll (Colours::black.withAlpha (0.25f));

    Path pp;
    pp.addRoundedRectangle (panel, u * 0.8f);
    DropShadow (Colours::black.withAlpha (0.75f), roundToInt (u * 2.5f), { 0, roundToInt (u * 0.8f) }).drawForPath (g, pp);
    const Colour bg = dark ? Colour (0xff131416) : Colour (0xfffbfaf7);
    g.setColour (bg);
    g.fillPath (pp);
    g.setColour (p.ink.withAlpha (0.14f));
    g.strokePath (pp, PathStrokeType (1.0f));

    // result count
    g.setColour (p.inkDim);
    g.setFont (NobLookAndFeel::font (u * 0.95f, true, 0.2f));
    g.drawText (String ((int) rows.size()) + (rows.size() == 1 ? " PRESET" : " PRESETS"),
                Rectangle<float> (panel.getX(), panel.getY() + u, panel.getWidth() - u, 2.4f * u), Justification::centredRight);

    const auto lit = p.glow (0.8f);
    const float rh = rowHeight();

    // categories
    {
        const auto a = categoryArea();
        g.setColour (dark ? Colours::white.withAlpha (0.03f) : Colours::black.withAlpha (0.03f));
        g.fillRoundedRectangle (a, u * 0.5f);
        for (int i = 0; i < categories.size(); ++i)
        {
            auto r = Rectangle<float> (a.getX(), a.getY() + (float) i * rh, a.getWidth(), rh);
            const bool sel = i == selectedCategory && search.getText().trim().isEmpty();
            if (sel || i == hoverCategory)
            {
                g.setColour (sel ? lit.withAlpha (dark ? 0.18f : 0.16f) : p.ink.withAlpha (0.05f));
                g.fillRoundedRectangle (r.reduced (u * 0.3f, u * 0.15f), u * 0.4f);
            }
            if (sel)
            {
                g.setColour (lit);
                g.fillRoundedRectangle (r.withWidth (u * 0.25f).reduced (0.0f, rh * 0.25f).translated (u * 0.3f, 0.0f), u * 0.1f);
            }
            g.setColour (sel ? p.ink : p.inkDim);
            g.setFont (NobLookAndFeel::font (rh * 0.42f, true, 0.12f));
            g.drawText (prettyCategory (categories[i]).toUpperCase(), r.withTrimmedLeft (u * 1.2f), Justification::centredLeft);
        }
    }

    // presets
    {
        const auto a = listArea();
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (a.toNearestInt());
        const int cur = pm.getCurrentIndex();
        const auto& all = pm.getPresets();
        const bool showCat = categories[selectedCategory] == "All" || search.getText().trim().isNotEmpty();

        for (int i = 0; i < (int) rows.size(); ++i)
        {
            auto r = Rectangle<float> (a.getX(), a.getY() + (float) i * rh - scroll, a.getWidth(), rh);
            if (r.getBottom() < a.getY() || r.getY() > a.getBottom()) continue;

            const auto& pr = all[(size_t) rows[(size_t) i]];
            const bool isCur = rows[(size_t) i] == cur;

            if (isCur || i == hoverRow)
            {
                g.setColour (isCur ? lit.withAlpha (dark ? 0.22f : 0.18f) : p.ink.withAlpha (0.05f));
                g.fillRoundedRectangle (r.reduced (0.0f, u * 0.15f), u * 0.4f);
            }
            g.setColour (isCur ? (dark ? Colours::white : p.ink) : p.ink.withAlpha (0.85f));
            g.setFont (NobLookAndFeel::font (rh * 0.45f, true, 0.03f));
            g.drawText (pr.name, r.withTrimmedLeft (u * 1.0f).withTrimmedRight (u * 8.0f), Justification::centredLeft, true);

            if (showCat)
            {
                g.setColour (p.inkDim);
                g.setFont (NobLookAndFeel::font (rh * 0.34f, true, 0.15f));
                g.drawText (pr.isFactory ? prettyCategory (pr.category).toUpperCase() : String ("USER"),
                            r.withTrimmedRight (u * 1.0f), Justification::centredRight);
            }
        }

        if (rows.empty())
        {
            g.setColour (p.inkDim);
            g.setFont (NobLookAndFeel::font (rh * 0.42f, true, 0.1f));
            g.drawText (categories[selectedCategory] == "User" && search.getText().isEmpty()
                            ? "No user presets yet. Use SAVE AS to create one." : "No matches",
                        a, Justification::centred);
        }

        // scrollbar
        const float total = (float) rows.size() * rh;
        if (total > a.getHeight())
        {
            const float barH = a.getHeight() * a.getHeight() / total;
            const float barY = a.getY() + (a.getHeight() - barH) * (scroll / (total - a.getHeight()));
            g.setColour (p.ink.withAlpha (0.2f));
            g.fillRoundedRectangle (a.getRight() - u * 0.35f, barY, u * 0.25f, barH, u * 0.12f);
        }
    }
}

void PresetBrowser::ActionButton::paintButton (Graphics& g, bool isOver, bool isDown)
{
    const auto& p = palB (*this);
    const bool dark = darkB (*this);
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const float corner = b.getHeight() * 0.2f;
    const float alpha = isEnabled() ? 1.0f : 0.35f;

    Colour fill = isPrimary ? p.accent : (dark ? Colour (0xff222327) : Colour (0xffeeebe5));
    if (getButtonText() == "CONFIRM?") fill = Colour (0xffd83a2a);
    if (isOver && isEnabled()) fill = fill.brighter (0.12f);
    if (isDown) fill = fill.darker (0.15f);
    g.setColour (fill.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (b, corner);

    g.setColour ((isPrimary || getButtonText() == "CONFIRM?" ? Colours::white : p.ink).withMultipliedAlpha (alpha));
    g.setFont (NobLookAndFeel::font (b.getHeight() * 0.38f, true, 0.22f));
    g.drawText (getButtonText(), b, Justification::centred);
}
