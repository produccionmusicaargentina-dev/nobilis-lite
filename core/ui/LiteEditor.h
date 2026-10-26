#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LiteProcessor.h"
#include "NobLookAndFeel.h"
#include "Widgets.h"
#include "PresetBar.h"
#include "PresetBrowser.h"
#include "LoginOverlay.h"
#include "license/UpdateChecker.h"

namespace nob::ui
{

class UpdatePill : public juce::Button
{
public:
    UpdatePill() : juce::Button ("update") {}
    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
};

class AccountBadge : public juce::Button
{
public:
    explicit AccountBadge (nobilis::LicenseManager& l) : juce::Button ("account"), lic (l) {}
    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
private:
    nobilis::LicenseManager& lic;
};

class DotsButton : public juce::Button
{
public:
    DotsButton() : juce::Button ("settings") {}
    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
};

// Flat engraved link in the footer ("SHARE", "MORE FROM NOBILIS ›")
class LinkButton : public juce::Button
{
public:
    LinkButton (const juce::String& t, bool accentDot = false) : juce::Button (t), dot (accentDot) { setButtonText (t); }
    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
private:
    bool dot;
};

//==============================================================================
// The chassis every Lite plugin shares, drawn on a 900 x 600 design canvas and
// scaled as one piece:
//
//   ( power )  [undo][redo]  [<][>][ preset          v ][save]  [A|B][COPY]  ( ... )
//   [ account ]                    P U L S U S                     LUX [==] NOX
//                                  — NOBILIS —
//                               TEMPO PUMP / LITE
//   ──────────────────────────────────────────────────────────────────────────────
//                         body: the plugin's own controls
//   ──────────────────────────────────────────────────────────────────────────────
//   NOBILIS LITE  FREE   ·  SHARE                       MORE FROM NOBILIS  IGNIS · ACIES ›
//
// A plugin derives from it, adds its controls with addToBody() and lays them
// out in layoutBody() using canvas coordinates (bodyArea() is the free space).
//==============================================================================
class LiteEditor : public juce::AudioProcessorEditor,
                   private juce::Timer
{
public:
    explicit LiteEditor (LiteProcessor&);
    ~LiteEditor() override;

    static constexpr int baseW = 900, baseH = 600;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setTheme (bool dark) { applyTheme (dark); }

    // Saves the share card (PNG for stories / posts + the preset file) on the Desktop.
    juce::File exportShareCard (const juce::File& folder = {});
    // A picture of the interface (tests, docs, web).
    juce::Image snapshot (float scale = 1.0f);

protected:
    // call at the end of the subclass constructor (after adding the body controls)
    void finishSetup();

    void addToBody (juce::Component& c) { content.addAndMakeVisible (c); }
    static juce::Rectangle<float> bodyArea() { return { 36.0f, 166.0f, (float) baseW - 72.0f, 352.0f }; }

    virtual void layoutBody() = 0;
    virtual void paintBody (juce::Graphics&, const NobPalette&, bool dark) { juce::ignoreUnused (dark); }
    virtual void paintBodyOver (juce::Graphics&, const NobPalette&, bool) {}
    // The logo (default: tracked letters). ADEPS inflates its own.
    virtual void paintLogo (juce::Graphics&, juce::Rectangle<float> area, const NobPalette&);
    virtual void refreshTexts() {}
    virtual void bodyTimer() {}            // 30 Hz, for meters and screens

    static juce::Rectangle<int> R (float x, float y, float w, float h) { return juce::Rectangle<float> (x, y, w, h).toNearestInt(); }

    LiteProcessor& lite;
    NobLookAndFeel lnf;

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (LiteEditor& e) : ed (e) {}
        void paint (juce::Graphics& g) override { ed.paintChassis (g); }
        void paintOverChildren (juce::Graphics& g) override { ed.paintVeil (g); }
    private:
        LiteEditor& ed;
    };

    void timerCallback() override;
    void paintChassis (juce::Graphics&);
    void paintVeil (juce::Graphics&);
    void applyTheme (bool dark);
    void rebuildTolex();
    void refreshCommonTexts();
    void showSettingsMenu();
    void showAccountMenu();
    void showMoreMenu();
    void showAbout();
    void shareNow();

    Content content { *this };
    PowerButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    PresetBar presetBar;
    BarButton undoBtn { BarButton::Undo }, redoBtn { BarButton::Redo }, copyBtn { BarButton::Copy };
    ABSwitch abSwitch;
    DotsButton dots;
    AccountBadge account;
    UpdatePill updatePill;
    SlideSwitch themeSwitch;
    LinkButton shareBtn { "SHARE", true }, moreBtn { "MORE FROM NOBILIS" };

    PresetBrowser presetBrowser;
    SaveOverlay saveOverlay;
    LoginOverlay loginOverlay;
    juce::SharedResourcePointer<nobilis::UpdateChecker> updates;
    std::unique_ptr<juce::TooltipWindow> tooltips;

    juce::Image tolexTile;
    float scale = 1.0f;
    bool sizeRestored = false, lastBypass = false, setupDone = false;
    juce::String lastAccount, toast;
    int toastTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiteEditor)
};

} // namespace nob::ui
