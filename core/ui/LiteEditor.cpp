#include "LiteEditor.h"
#include "Localisation.h"
#include "Diagnostics.h"
#include "license/LicenseKeys.h"

using namespace juce;

namespace nob::ui
{

namespace
{
    // ---- design canvas (900 x 600), symmetric around x = 450 ---------------------------
    constexpr float W = (float) LiteEditor::baseW, H = (float) LiteEditor::baseH;
    constexpr float cx = W * 0.5f;
    constexpr float panelX = 14, panelY = 14, panelW = W - 28, panelH = H - 28;
    constexpr float rowY = 30, rowH = 26;
    constexpr float presetW = 296, presetX = cx - presetW * 0.5f;
    constexpr float sideCX = 132;
    constexpr float dividerY = 154, footerY = 528;

    void drawScrew (Graphics& g, Point<float> c, float r, float slotAngle)
    {
        ColourGradient grad (Colour (0xffe9e9e6), c.x - r, c.y - r, Colour (0xff5f5f5c), c.x + r, c.y + r, false);
        g.setGradientFill (grad);
        g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c));
        g.setColour (Colours::black.withAlpha (0.55f));
        g.drawEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c), 0.8f);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawLine (Line<float> (c.getPointOnCircumference (r * 0.75f, slotAngle),
                                 c.getPointOnCircumference (r * 0.75f, slotAngle + MathConstants<float>::pi)),
                    jmax (1.0f, r * 0.28f));
    }

    String kindLabel (const nobilis::LicenseManager& lic)
    {
        const auto kind = lic.getKind();
        if (kind == "admin") return "ADMIN";
        if (kind == "nfr") return "NFR";
        return lic.isUnlocked() ? TRANS ("FREE  /  LITE PACK") : String();
    }

    const char* moreProducts[][3] =
    {
        { "IGNIS",  "Analog saturator",      "plugins/ignis" },
        { "ACIES",  "Dynamic equalizer",     "plugins/acies" },
        { "GRAVIS", "Instrument",            "plugins/gravis" },
    };
}

//==============================================================================
LiteEditor::LiteEditor (LiteProcessor& p)
    : AudioProcessorEditor (p), lite (p),
      presetBar (*p.presets), account (*p.licence),
      presetBrowser (*p.presets), loginOverlay (*p.licence)
{
    diag::step ("Editor: constructor start");
    i18n::applySavedLanguage();
    const bool dark = (bool) lite.apvts.state.getProperty (LiteProcessor::uiThemeKey, true);
    lnf.setDark (dark);
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (power);
    bypassAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (lite.apvts, CommonID::bypass, power);
    power.onClick = [this] { content.repaint(); };

    content.addAndMakeVisible (presetBar);
    presetBar.onSaveAs = [this] { saveOverlay.show (lite.presets->getCurrentName()); };
    presetBar.onBrowse = [this]
    {
        if (presetBrowser.isVisible()) { presetBrowser.close(); return; }
        presetBrowser.open ({ presetX - 150, rowY + rowH + 6, presetW + 300, 380 });
    };
    presetBrowser.onSaveAs = [this] { saveOverlay.show (lite.presets->getCurrentName()); };
    presetBrowser.onChanged = [this] { presetBar.refresh(); };

    for (auto* b : { (Button*) &undoBtn, (Button*) &redoBtn, (Button*) &abSwitch, (Button*) &copyBtn, (Button*) &dots })
        content.addAndMakeVisible (b);
    undoBtn.onClick  = [this] { lite.history->undo(); };
    redoBtn.onClick  = [this] { lite.history->redo(); };
    abSwitch.onClick = [this] { lite.history->toggleAB(); abSwitch.onB = lite.history->isOnB(); abSwitch.repaint(); };
    copyBtn.onClick  = [this] { lite.history->copyToOther(); };
    dots.onClick     = [this] { showSettingsMenu(); };

    content.addAndMakeVisible (account);
    account.onClick = [this] { showAccountMenu(); };
    content.addChildComponent (updatePill);
    updatePill.onClick = [this] { URL (updates->getDownloadUrl()).launchInDefaultBrowser(); };

    themeSwitch.setToggleState (dark, dontSendNotification);
    themeSwitch.onClick = [this] { applyTheme (themeSwitch.getToggleState()); };
    content.addAndMakeVisible (themeSwitch);

    content.addAndMakeVisible (shareBtn);
    content.addAndMakeVisible (moreBtn);
    shareBtn.onClick = [this] { shareNow(); };
    moreBtn.onClick = [this] { showMoreMenu(); };
}

void LiteEditor::finishSetup()
{
    // overlays go on top of the body controls
    content.addChildComponent (presetBrowser);
    saveOverlay.existsCheck = [this] (const String& n) { return lite.presets->userPresetExists (n); };
    saveOverlay.onConfirm = [this] (const String& n) { lite.presets->saveUserPreset (n); presetBar.refresh(); };
    content.addChildComponent (saveOverlay);
    content.addChildComponent (loginOverlay);
    loginOverlay.toFront (false);

    tooltips = std::make_unique<TooltipWindow> (&content, 600);
    refreshCommonTexts();
    refreshTexts();
    rebuildTolex();
    applyTheme (lnf.isDark());

    // resizable, fixed aspect ratio (read the saved width first: setResizeLimits calls resized())
    const int savedW = (int) lite.apvts.state.getProperty (LiteProcessor::uiWidthKey, 0);
    setResizable (true, true);
    setResizeLimits (baseW * 3 / 5, baseH * 3 / 5, baseW * 2, baseH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / baseH);
    const int w = savedW > 0 ? jlimit (baseW * 3 / 5, baseW * 2, savedW) : baseW;
    setupDone = true;
    setSize (w, roundToInt ((float) w * baseH / baseW));
    sizeRestored = true;
    startTimerHz (30);
    diag::step ("Editor: constructor end");
}

LiteEditor::~LiteEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void LiteEditor::refreshCommonTexts()
{
    power.setTooltip (TRANS ("Bypass (also follows the DAW bypass)"));
    undoBtn.setTooltip (TRANS ("Undo"));
    redoBtn.setTooltip (TRANS ("Redo"));
    abSwitch.setTooltip (TRANS ("Compare two settings (A / B)"));
    copyBtn.setTooltip (TRANS ("Copy the current settings to the other slot"));
    dots.setTooltip (TRANS ("Settings: language, size, about"));
    themeSwitch.setTooltip (TRANS ("LUX / NOX theme"));
    account.setTooltip (TRANS ("Your Nobilis account"));
    shareBtn.setButtonText (TRANS ("SHARE"));
    shareBtn.setTooltip (TRANS ("Save a picture of this setting and its preset, ready for stories and posts"));
    moreBtn.setButtonText (TRANS ("MORE FROM NOBILIS"));
    moreBtn.setTooltip (TRANS ("The full Nobilis plugins"));
    updatePill.setTooltip (TRANS ("A new version is available. Click to download it."));
}

void LiteEditor::applyTheme (bool dark)
{
    lnf.setDark (dark);
    lite.apvts.state.setProperty (LiteProcessor::uiThemeKey, dark, nullptr);
    themeSwitch.setToggleState (dark, dontSendNotification);
    rebuildTolex();
    sendLookAndFeelChange();
    content.repaint();
}

void LiteEditor::rebuildTolex()
{
    const auto& p = lnf.pal();
    tolexTile = Image (Image::RGB, 192, 192, true, SoftwareImageType());
    Graphics tg (tolexTile);
    tg.fillAll (p.tolex);
    Random rnd (42);
    for (int i = 0; i < 5200; ++i)
    {
        const float x = rnd.nextFloat() * 192.0f, y = rnd.nextFloat() * 192.0f;
        const float r = 0.6f + rnd.nextFloat() * 1.6f;
        tg.setColour ((rnd.nextBool() ? p.tolexSpeck : p.tolex.darker (0.35f)).withAlpha (0.25f + rnd.nextFloat() * 0.35f));
        tg.fillEllipse (x, y, r, r * (0.6f + rnd.nextFloat() * 0.8f));
    }
}

//==============================================================================
void LiteEditor::paint (Graphics& g) { g.fillAll (Colours::black); }

void LiteEditor::resized()
{
    if (! setupDone) return;
    scale = (float) getWidth() / (float) baseW;
    content.setBounds (0, 0, baseW, baseH);
    content.setTransform (AffineTransform::scale (scale));
    lnf.menuScale = scale;
    if (sizeRestored)
        lite.apvts.state.setProperty (LiteProcessor::uiWidthKey, getWidth(), nullptr);

    power.setBounds (R (40, rowY, rowH, rowH));
    dots.setBounds (R (W - 40 - rowH, rowY, rowH, rowH));
    presetBar.setBounds (R (presetX, rowY, presetW, rowH));
    undoBtn.setBounds (R (presetX - 10 - 104, rowY, 50, rowH));
    redoBtn.setBounds (R (presetX - 10 - 50, rowY, 50, rowH));
    abSwitch.setBounds (R (presetX + presetW + 10, rowY, 50, rowH));
    copyBtn.setBounds (R (presetX + presetW + 10 + 54, rowY, 50, rowH));

    account.setBounds (R (sideCX - 88, 76, 176, 38));
    updatePill.setBounds (R (sideCX - 80, 120, 160, 20));
    themeSwitch.setBounds (R (W - sideCX - 40, 78, 80, 26));

    shareBtn.setBounds (R (210, footerY + 8, 110, 22));
    moreBtn.setBounds (R (W - 40 - 330, footerY + 8, 330, 22));

    layoutBody();

    presetBrowser.close();
    presetBrowser.setBounds (content.getLocalBounds());
    saveOverlay.setBounds (content.getLocalBounds());
    loginOverlay.setBounds (content.getLocalBounds());
}

void LiteEditor::paintLogo (Graphics& g, Rectangle<float> area, const NobPalette& p)
{
    g.setColour (p.ink);
    g.setFont (NobLookAndFeel::font (40.0f, true, 0.30f));
    // tracking adds space after the last letter: nudge right to centre optically
    g.drawText (product().name, area.translated (6.0f, 0.0f), Justification::centred);
}

void LiteEditor::paintChassis (Graphics& g)
{
    const auto& p = lnf.pal();
    const bool dark = lnf.isDark();
    const auto bounds = content.getLocalBounds().toFloat();

    // cabinet (tolex)
    g.fillAll (Colours::black);
    {
        Path cab;
        cab.addRoundedRectangle (bounds, 14.0f);
        g.setFillType (FillType (tolexTile, AffineTransform()));
        g.fillPath (cab);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (dark ? 0.06f : 0.20f), 0, 0,
                                           Colours::black.withAlpha (dark ? 0.35f : 0.10f), 0, bounds.getBottom(), false));
        g.fillPath (cab);
    }
    // chrome corners
    {
        const float cs = 36.0f;
        const Point<float> corners[] = { { 0, 0 }, { bounds.getRight() - cs, 0 }, { 0, bounds.getBottom() - cs }, { bounds.getRight() - cs, bounds.getBottom() - cs } };
        for (auto c : corners)
        {
            auto r = Rectangle<float> (c.x, c.y, cs, cs);
            ColourGradient chrome (Colour (0xfff1f1ee), r.getX(), r.getY(), Colour (0xff6a6a67), r.getRight(), r.getBottom(), false);
            chrome.addColour (0.5, Colour (0xffb7b7b3));
            g.setGradientFill (chrome);
            g.fillRoundedRectangle (r, 12.0f);
            g.setColour (Colours::black.withAlpha (0.5f));
            g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
            drawScrew (g, { c.x < 1 ? r.getX() + 8.0f : r.getRight() - 8.0f, c.y < 1 ? r.getY() + 8.0f : r.getBottom() - 8.0f }, 3.0f, 0.6f);
        }
    }
    // front panel
    const auto panel = Rectangle<float> (panelX, panelY, panelW, panelH);
    {
        Path pp;
        pp.addRoundedRectangle (panel, 8.0f);
        DropShadow (Colours::black.withAlpha (0.7f), 10, { 0, 3 }).drawForPath (g, pp);
        g.setGradientFill (ColourGradient (p.panel.brighter (dark ? 0.05f : 0.0f), 0, panel.getY(),
                                           p.panel.darker (dark ? 0.15f : 0.04f), 0, panel.getBottom(), false));
        g.fillPath (pp);
        Random rnd (7);
        for (float y = panel.getY(); y < panel.getBottom(); y += 1.5f)
        {
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (0.012f + rnd.nextFloat() * 0.015f));
            g.drawHorizontalLine ((int) y, panel.getX(), panel.getRight());
        }
        g.setColour (p.panelEdge);
        g.drawRoundedRectangle (panel, 8.0f, 1.5f);
        g.setColour (p.piping.withAlpha (0.55f));
        g.drawRoundedRectangle (panel.reduced (8.0f), 5.0f, 1.2f);
        for (auto c : { panel.getTopLeft().translated (16, 16), panel.getTopRight().translated (-16, 16),
                        panel.getBottomLeft().translated (16, -16), panel.getBottomRight().translated (-16, -16) })
            drawScrew (g, c, 4.0f, 0.8f);
    }

    // logo
    paintLogo (g, Rectangle<float> (cx - 200, 66, 400, 48), p);
    g.setColour (p.inkDim);
    g.setFont (NobLookAndFeel::font (9.5f, true, 0.55f));
    g.drawText ("NOBILIS", Rectangle<float> (cx - 60 + 3, 116, 120, 13), Justification::centred);
    g.setColour (p.accent);
    g.fillRect (Rectangle<float> (cx - 66, 121.5f, 16, 2.0f));
    g.fillRect (Rectangle<float> (cx + 50, 121.5f, 16, 2.0f));
    g.setColour (p.inkDim.withAlpha (0.8f));
    g.setFont (NobLookAndFeel::font (7.5f, true, 0.3f));
    g.drawText (product().tagline, Rectangle<float> (cx - 160, 134, 320, 11), Justification::centred);

    // theme labels
    g.setFont (NobLookAndFeel::font (9.5f, true, 0.4f));
    g.setColour (themeSwitch.getToggleState() ? p.inkDim : p.ink);
    g.drawText ("LUX", Rectangle<float> (W - sideCX - 40, 108, 40, 14), Justification::centred);
    g.setColour (themeSwitch.getToggleState() ? p.ink : p.inkDim);
    g.drawText ("NOX", Rectangle<float> (W - sideCX, 108, 40, 14), Justification::centred);

    // dividers
    g.setColour (p.ink.withAlpha (0.18f));
    g.fillRect (Rectangle<float> (36, dividerY, W - 72, 1));
    g.fillRect (Rectangle<float> (36, footerY, W - 72, 1));

    // footer: pack mark + FREE pill
    {
        g.setColour (p.ink);
        g.setFont (NobLookAndFeel::font (9.5f, true, 0.42f));
        g.drawText (packName, Rectangle<float> (40, footerY + 8, 110, 22), Justification::centredLeft);
        const auto pill = Rectangle<float> (152, footerY + 12, 40, 14);
        g.setColour (p.accent);
        g.drawRoundedRectangle (pill, 7.0f, 1.0f);
        g.setFont (NobLookAndFeel::font (7.5f, true, 0.3f));
        g.drawText (TRANS ("FREE"), pill.translated (1.0f, 0.0f), Justification::centred);
    }

    paintBody (g, p, dark);

    if (toastTicks > 0)
    {
        const auto t = Rectangle<float> (cx - 200, footerY - 34, 400, 26);
        g.setColour ((dark ? Colours::black : Colours::white).withAlpha (0.85f));
        g.fillRoundedRectangle (t, 6.0f);
        g.setColour (p.accent);
        g.drawRoundedRectangle (t, 6.0f, 1.0f);
        g.setColour (p.ink);
        g.setFont (NobLookAndFeel::font (9.5f, true, 0.08f));
        g.drawFittedText (toast, t.reduced (10, 2).toNearestInt(), Justification::centred, 1);
    }
}

void LiteEditor::paintVeil (Graphics& g)
{
    paintBodyOver (g, lnf.pal(), lnf.isDark());
    if (! power.getToggleState()) return;
    g.setColour ((lnf.isDark() ? Colours::black : Colours::white).withAlpha (0.45f));
    g.fillRect (Rectangle<float> (20, dividerY + 2, W - 40, footerY - dividerY - 4));
    g.setColour (lnf.pal().accent);
    g.setFont (NobLookAndFeel::font (9.5f, true, 0.35f));
    g.drawText ("BYPASS", Rectangle<float> (72, rowY, 80, rowH), Justification::centredLeft);
}

void LiteEditor::timerCallback()
{
    if (const bool upd = updates->hasUpdate(); upd != updatePill.isVisible())
    {
        updatePill.setButtonText (TRANS ("NEW VERSION") + " " + updates->getLatestVersion());
        updatePill.setVisible (upd);
    }
    presetBar.refresh();
    auto& hist = *lite.history;
    hist.poll();
    undoBtn.setEnabled (hist.canUndo());
    redoBtn.setEnabled (hist.canRedo());
    if (abSwitch.onB != hist.isOnB()) { abSwitch.onB = hist.isOnB(); abSwitch.repaint(); }

    const bool bypassed = power.getToggleState();
    if (bypassed != lastBypass) { lastBypass = bypassed; content.repaint(); }

    const auto acc = lite.licence->getEmail() + "|" + lite.licence->getKind() + "|" + String ((int) lite.licence->isUnlocked());
    if (acc != lastAccount) { lastAccount = acc; account.repaint(); }

    if (toastTicks > 0 && --toastTicks == 0)
        content.repaint();

    bodyTimer();
}

//==============================================================================
Image LiteEditor::snapshot (float s)
{
    presetBrowser.close();
    return content.createComponentSnapshot (content.getLocalBounds(), true, s);
}

File LiteEditor::exportShareCard (const File& folderIn)
{
    const auto& p = lnf.pal();
    const bool dark = lnf.isDark();
    const String plugin = product().name;
    const String preset = lite.presets->getCurrentName();

    // 1080 x 1350: the portrait size Instagram shows uncropped in the feed
    Image card (Image::RGB, 1080, 1350, true);
    {
        Graphics g (card);
        g.setFillType (FillType (tolexTile, AffineTransform::scale (1.5f)));
        g.fillAll();
        g.setColour ((dark ? Colours::black : Colours::white).withAlpha (dark ? 0.35f : 0.25f));
        g.fillAll();

        g.setColour (p.ink);
        const auto brandFont = NobLookAndFeel::font (34.0f, true, 0.55f);
        g.setFont (brandFont);
        g.drawText ("NOBILIS", Rectangle<int> (0, 70, 1080, 44), Justification::centred);
        // the dashes sit outside the measured word (the old fixed positions cut into the N and the S)
        const int half = roundToInt (GlyphArrangement::getStringWidth (brandFont, "NOBILIS") * 0.5f);
        g.setColour (p.accent);
        g.fillRect (Rectangle<int> (540 - half - 22 - 34, 89, 34, 4));
        g.fillRect (Rectangle<int> (540 + half + 22, 89, 34, 4));

        const float s = 1000.0f / (float) baseW;
        // the picture is meant to be posted: never show who is signed in (email / account chip)
        const bool accountWasVisible = account.isVisible();
        account.setVisible (false);
        auto shot = content.createComponentSnapshot (content.getLocalBounds(), true, s * 1.25f);
        account.setVisible (accountWasVisible);
        const auto dest = Rectangle<float> (40.0f, 170.0f, 1000.0f, (float) baseH * s);
        Path frame;
        frame.addRoundedRectangle (dest, 16.0f);
        DropShadow (Colours::black.withAlpha (0.8f), 40, { 0, 16 }).drawForPath (g, frame);
        g.drawImage (shot, dest, RectanglePlacement::stretchToFit);

        g.setColour (p.inkDim);
        g.setFont (NobLookAndFeel::font (26.0f, true, 0.4f));
        g.drawText (plugin + "  /  " + TRANS ("PRESET"), Rectangle<int> (0, 900, 1080, 34), Justification::centred);
        g.setColour (p.ink);
        g.setFont (NobLookAndFeel::font (64.0f, true, 0.04f));
        g.drawFittedText (preset, Rectangle<int> (60, 944, 960, 90), Justification::centred, 1);

        const auto pill = Rectangle<float> (250, 1110, 580, 86);
        g.setColour (p.accent);
        g.fillRoundedRectangle (pill, 43.0f);
        g.setColour (dark ? Colours::white : Colour (0xff1a0903));
        g.setFont (NobLookAndFeel::font (30.0f, true, 0.2f));
        g.drawText (TRANS ("FREE AT") + " NOBILIS.AR/LITE", pill, Justification::centred);

        g.setColour (p.inkDim);
        g.setFont (NobLookAndFeel::font (20.0f, true, 0.3f));
        g.drawText ("PULSUS  /  ADEPS  /  LATUS  /  VETUS", Rectangle<int> (0, 1230, 1080, 30), Justification::centred);
    }

    auto folder = folderIn;
    if (folder == File())
        folder = File::getSpecialLocation (File::userDesktopDirectory).getChildFile ("Nobilis " + plugin);
    folder.createDirectory();
    const auto base = File::createLegalFileName (plugin + " - " + preset);
    auto png = folder.getChildFile (base + ".png");
    png.deleteFile();
    if (FileOutputStream out (png); out.openedOk())
        PNGImageFormat().writeImageToStream (card, out);

    // the preset file next to it, so whoever sees the post can load the same sound
    if (const int cur = lite.presets->getCurrentIndex(); cur < 0 || lite.presets->getPresets()[(size_t) cur].isFactory || lite.presets->isModified())
        lite.presets->saveUserPreset (preset);
    const auto presetFile = lite.presets->getUserPresetFile (preset);
    if (presetFile.existsAsFile())
        presetFile.copyFileTo (folder.getChildFile (presetFile.getFileName()));
    return png;
}

void LiteEditor::shareNow()
{
    const auto png = exportShareCard();
    if (png.existsAsFile())
    {
        png.revealToUser();
        toast = TRANS ("Saved on your Desktop. Tag @nobilis.ar when you post it!");
    }
    else
        toast = TRANS ("Could not save the picture.");
    toastTicks = 30 * 5;
    content.repaint();
}

//==============================================================================
void LiteEditor::showSettingsMenu()
{
    PopupMenu menu, lang, size;
    menu.setLookAndFeel (&lnf);
    for (const auto& l : i18n::languages())
    {
        const String code = l.code;
        lang.addItem (String::fromUTF8 (l.nativeName), true, code == i18n::currentCode(), [this, code]
        {
            i18n::setLanguage (code);
            refreshCommonTexts();
            refreshTexts();
            content.repaint();
        });
    }
    for (int pct : { 60, 75, 90, 100, 115, 130, 150, 175, 200 })
    {
        const int w = baseW * pct / 100;
        size.addItem (String (pct) + " %", true, std::abs (getWidth() - w) < 4, [this, w] { setSize (w, roundToInt ((float) w * baseH / baseW)); });
    }
    menu.addSubMenu (TRANS ("Language"), lang);
    menu.addSubMenu (TRANS ("Interface size"), size);
    menu.addSeparator();
    menu.addItem (TRANS ("Share this setting (picture + preset)"), [this] { shareNow(); });
    menu.addItem (TRANS ("Check for updates on the web"), [this] { URL (updates->getDownloadUrl()).launchInDefaultBrowser(); });
    menu.addItem (TRANS ("About") + " " + String (product().name) + "...", [this] { showAbout(); });
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&dots));
}

void LiteEditor::showAbout()
{
    const String body = TRANS ("Version") + " " + JucePlugin_VersionString + "\n\n"
                      + TRANS (product().about) + "\n\n"
                      + String (packName) + NOB_TEXT (" \xc2\xb7 PULSUS \xc2\xb7 ADEPS \xc2\xb7 LATUS \xc2\xb7 VETUS\n\n")
                      + NOB_TEXT ("Nobilis \xc2\xb7 ") + TRANS ("Music production") + NOB_TEXT (" \xc2\xb7 Argentina\n")
                      + String (kLicenseServer).fromFirstOccurrenceOf ("://", false, false) + "\n\n"
                      + "Made with JUCE. VST is a registered trademark of Steinberg Media Technologies GmbH. "
                        "Audio Units is a trademark of Apple Inc.\n\n"
                      + NOB_TEXT ("\xc2\xa9 ") + String (Time::getCurrentTime().getYear()) + " Nobilis.";
    auto opts = MessageBoxOptions().withIconType (MessageBoxIconType::NoIcon).withTitle (product().name).withMessage (body)
                    .withButton (TRANS ("Visit the website")).withButton (TRANS ("Close")).withAssociatedComponent (this);
    AlertWindow::showAsync (opts, [] (int r) { if (r == 1) URL (String (kLicenseServer) + "/#plugins").launchInDefaultBrowser(); });
}

void LiteEditor::showMoreMenu()
{
    PopupMenu m;
    m.setLookAndFeel (&lnf);
    m.addSectionHeader (TRANS ("The full Nobilis plugins"));
    int id = 1;
    for (auto& pr : moreProducts)
        m.addItem (id++, String (pr[0]) + NOB_TEXT ("  \xc2\xb7  ") + TRANS (pr[1]));
    m.addSeparator();
    m.addItem (10, TRANS ("All plugins and bundles"));
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&moreBtn), [] (int r)
    {
        if (r >= 1 && r <= (int) std::size (moreProducts))
            URL (String (kLicenseServer) + "/?utm_source=lite&utm_medium=plugin&utm_campaign=" + String (product().name).toLowerCase()
                 + "#" + moreProducts[r - 1][2]).launchInDefaultBrowser();
        if (r == 10)
            URL (String (kLicenseServer) + "/?utm_source=lite&utm_medium=plugin#plugins").launchInDefaultBrowser();
    });
}

void LiteEditor::showAccountMenu()
{
    auto& lic = *lite.licence;
    PopupMenu m;
    m.setLookAndFeel (&lnf);
    const auto email = lic.getEmail();
    m.addSectionHeader (email.isNotEmpty() ? email : TRANS ("Not signed in"));
    m.addSeparator();
    m.addItem (1, TRANS ("My account on the web"));
    m.addItem (3, TRANS ("More Nobilis plugins"));
    m.addItem (4, TRANS ("About") + " " + String (product().name) + "...");
    m.addSeparator();
    m.addItem (2, TRANS ("Sign out on this computer"), lic.getState() == nobilis::LicenseManager::State::active);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&account),
                     [safe = Component::SafePointer<LiteEditor> (this)] (int r)
                     {
                         if (r == 1) URL (String (kLicenseServer) + "/cuenta/").launchInDefaultBrowser();
                         if (r == 3) URL (String (kLicenseServer) + "/#plugins").launchInDefaultBrowser();
                         if (safe == nullptr) return;
                         if (r == 2) safe->lite.licence->logout();
                         if (r == 4) safe->showAbout();
                     });
}

//==============================================================================
void UpdatePill::paintButton (Graphics& g, bool isOver, bool isDown)
{
    const auto accent = palOf (*this).accent;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = b.getHeight() * 0.5f;
    g.setColour (accent.withAlpha (isDown ? 0.35f : (isOver ? 0.25f : 0.14f)));
    g.fillRoundedRectangle (b, corner);
    g.setColour (accent);
    g.drawRoundedRectangle (b, corner, 1.2f);
    const float d = b.getHeight() * 0.28f;
    g.fillEllipse (b.getX() + b.getHeight() * 0.45f, b.getCentreY() - d * 0.5f, d, d);
    g.setFont (NobLookAndFeel::font (b.getHeight() * 0.42f, true, 0.12f));
    g.drawFittedText (getButtonText() + NOB_TEXT ("  \xe2\x80\xba"),
                      b.withTrimmedLeft (b.getHeight() * 0.9f).withTrimmedRight (b.getHeight() * 0.4f).toNearestInt(), Justification::centred, 1);
}

void AccountBadge::paintButton (Graphics& g, bool isOver, bool)
{
    const auto& p = palOf (*this);
    const bool dark = darkOf (*this);
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (dark ? Colours::black.withAlpha (isOver ? 0.38f : 0.28f) : Colours::black.withAlpha (isOver ? 0.08f : 0.05f));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (p.ink.withAlpha (isOver ? 0.3f : 0.14f));
    g.drawRoundedRectangle (b, 6.0f, 1.0f);

    auto icon = b.removeFromLeft (b.getHeight()).reduced (10.0f);
    g.setColour (p.inkDim);
    g.fillEllipse (icon.withSizeKeepingCentre (icon.getWidth() * 0.5f, icon.getWidth() * 0.5f).translated (0, -icon.getHeight() * 0.2f));
    Path body;
    body.addCentredArc (icon.getCentreX(), icon.getBottom(), icon.getWidth() * 0.45f, icon.getHeight() * 0.45f, 0.0f,
                        -MathConstants<float>::halfPi, MathConstants<float>::halfPi, true);
    g.fillPath (body);

    const auto email = lic.getEmail();
    const auto kind = kindLabel (lic);
    g.setColour (p.ink);
    g.setFont (NobLookAndFeel::font (9.5f, true, 0.04f));
    g.drawFittedText (email.isNotEmpty() ? email : TRANS ("NOT SIGNED IN"), b.withTrimmedRight (8).removeFromTop (b.getHeight() * 0.55f).toNearestInt(),
                      Justification::bottomLeft, 1, 0.7f);
    g.setColour (lic.isUnlocked() ? p.accent : p.inkDim);
    g.setFont (NobLookAndFeel::font (8.0f, true, 0.3f));
    g.drawText (kind.isNotEmpty() ? kind : String ("-"), b.withTrimmedTop (b.getHeight() * 0.58f), Justification::topLeft);
}

void DotsButton::paintButton (Graphics& g, bool isOver, bool isDown)
{
    const auto& p = palOf (*this);
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    ColourGradient ring (Colour (0xffe9e9e6), b.getX(), b.getY(), Colour (0xff6a6a67), b.getRight(), b.getBottom(), false);
    g.setGradientFill (ring);
    g.fillEllipse (b);
    auto face = b.reduced (2.5f);
    g.setColour (darkOf (*this) ? Colour (0xff161616).brighter (isOver ? 0.15f : 0.0f) : Colour (0xfff2f0ea).darker (isOver ? 0.05f : 0.0f));
    g.fillEllipse (face);
    g.setColour (isDown ? p.accent : p.ink.withAlpha (0.8f));
    const float d = 2.4f;
    for (int i = -1; i <= 1; ++i)
        g.fillEllipse (Rectangle<float> (d, d).withCentre ({ face.getCentreX() + (float) i * 5.0f, face.getCentreY() }));
}

void LinkButton::paintButton (Graphics& g, bool isOver, bool isDown)
{
    const auto& p = palOf (*this);
    auto b = getLocalBounds().toFloat();
    const auto f = NobLookAndFeel::font (9.0f, true, 0.32f);
    g.setFont (f);
    String label = getButtonText();
    if (! dot) label << NOB_TEXT ("  \xe2\x80\xba");
    const float tw = GlyphArrangement::getStringWidth (f, label);
    float x = dot ? b.getX() : b.getRight() - tw;
    if (dot)
    {
        // little "share" glyph: three connected nodes
        const float cy = b.getCentreY();
        Point<float> a (x + 3, cy), b1 (x + 11, cy - 4.5f), b2 (x + 11, cy + 4.5f);
        g.setColour (isOver ? p.accent : p.inkDim);
        g.drawLine ({ a, b1 }, 1.2f);
        g.drawLine ({ a, b2 }, 1.2f);
        for (auto q : { a, b1, b2 }) g.fillEllipse (Rectangle<float> (4.2f, 4.2f).withCentre (q));
        x += 20.0f;
    }
    g.setColour (isDown ? p.accent : (isOver ? p.ink : p.inkDim));
    g.drawText (label, Rectangle<float> (x, b.getY(), tw + 4.0f, b.getHeight()), Justification::centredLeft);
    if (isOver)
        g.fillRect (Rectangle<float> (x, b.getCentreY() + 7.0f, tw, 1.0f));
}

} // namespace nob::ui
