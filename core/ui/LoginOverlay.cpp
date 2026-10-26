#include "LoginOverlay.h"
#include "LicenseKeys.h"
#include "Product.h"
#include "NobLookAndFeel.h"

using namespace juce;
using State = nobilis::LicenseManager::State;

static const NobPalette& palFor (Component& c)
{
    if (auto* laf = dynamic_cast<NobLookAndFeel*> (&c.getLookAndFeel()))
        return laf->pal();
    static const NobPalette fallback = NobPalette::nox();
    return fallback;
}
static bool darkFor (Component& c)
{
    if (auto* laf = dynamic_cast<NobLookAndFeel*> (&c.getLookAndFeel()))
        return laf->isDark();
    return true;
}

LoginOverlay::LoginOverlay (nobilis::LicenseManager& l) : lic (l)
{
    setInterceptsMouseClicks (true, true);
    addAndMakeVisible (primary);
    addAndMakeVisible (secondary);

    primary.onClick = [this]
    {
        if (lic.getState() == State::waitingBrowser) lic.reopenBrowser();
        else                                          lic.beginLogin();
    };
    secondary.onClick = [this]
    {
        if (lic.getState() == State::waitingBrowser) lic.cancelLogin();
        else URL (String (kLicenseServer) + "/cuenta/?modo=registro&interes=LITE").launchInDefaultBrowser();
    };

    lic.addChangeListener (this);
    refresh();
}

LoginOverlay::~LoginOverlay()
{
    lic.removeChangeListener (this);
}

void LoginOverlay::changeListenerCallback (ChangeBroadcaster*) { refresh(); }

void LoginOverlay::refresh()
{
    shown = lic.getState();
    setVisible (shown != State::active);
    if (isVisible())
        toFront (false);

    const bool waiting = shown == State::waitingBrowser;
    const bool hasCode = lic.getUserCode().isNotEmpty();
    primary.setButtonText (waiting ? TRANS ("OPEN AGAIN") : TRANS ("SIGN IN FREE"));
    secondary.setButtonText (waiting ? TRANS ("CANCEL") : TRANS ("CREATE ACCOUNT"));
    primary.setVisible (shown != State::checking && (! waiting || hasCode));
    secondary.setVisible (shown != State::checking);
    resized();
    repaint();
}

Rectangle<float> LoginOverlay::card() const
{
    const float w = jmin ((float) getWidth() * 0.62f, (float) getHeight() * 0.85f);
    return getLocalBounds().toFloat().withSizeKeepingCentre (w, w * 0.62f);
}

void LoginOverlay::resized()
{
    const auto c = card();
    const float u = c.getHeight() / 10.0f;
    auto row = c.withTrimmedTop (u * 7.4f).withHeight (u * 1.25f).reduced (u * 1.2f, 0);
    const float gap = u * 0.5f;
    if (secondary.isVisible() && primary.isVisible())
    {
        const float half = (row.getWidth() - gap) * 0.5f;
        primary.setBounds (row.removeFromLeft (half).toNearestInt());
        row.removeFromLeft (gap);
        secondary.setBounds (row.toNearestInt());
    }
    else
    {
        primary.setBounds (row.toNearestInt());
        secondary.setBounds (row.toNearestInt());
    }
}

void LoginOverlay::paint (Graphics& g)
{
    const auto& p = palFor (*this);
    g.fillAll (Colours::black.withAlpha (darkFor (*this) ? 0.72f : 0.55f));

    const auto c = card();
    const float u = c.getHeight() / 10.0f;
    Path cp;
    cp.addRoundedRectangle (c, u * 0.5f);
    DropShadow (Colours::black.withAlpha (0.8f), roundToInt (u * 2), { 0, roundToInt (u * 0.6f) }).drawForPath (g, cp);
    g.setColour (p.panel);
    g.fillPath (cp);
    g.setColour (p.ink.withAlpha (0.25f));
    g.strokePath (cp, PathStrokeType (1.0f));

    auto text = c.reduced (u * 1.0f, 0);
    g.setColour (p.ink);
    g.setFont (NobLookAndFeel::font (u * 1.15f, true, 0.30f));
    g.drawText (nob::product().name, text.withY (c.getY() + u * 0.8f).withHeight (u * 1.4f), Justification::centred);
    g.setColour (p.accent);
    g.fillRect (Rectangle<float> (c.getCentreX() - u * 0.8f, c.getY() + u * 2.35f, u * 1.6f, 2.0f));

    String title, body;
    switch (shown)
    {
        case State::checking:       title = TRANS ("Checking your licence..."); break;
        case State::waitingBrowser: title = TRANS ("Confirm in your browser");
                                    body = lic.getUserCode().isNotEmpty() ? TRANS ("Sign in on the page that just opened and press Activate.")
                                                                          : TRANS ("Connecting..."); break;
        case State::error:          title = TRANS ("Could not sign in"); body = lic.getMessage(); break;
        case State::loggedOut:
        case State::active:
        default:                    title = TRANS ("Sign in to use") + " " + nob::product().name;
                                    body = lic.getMessage().isNotEmpty() ? lic.getMessage()
                                                                         : TRANS ("Free forever with your Nobilis account. One sign-in unlocks the whole Lite pack and then works offline."); break;
    }

    g.setColour (p.ink);
    g.setFont (NobLookAndFeel::font (u * 0.62f, true, 0.06f));
    g.drawFittedText (title, text.withY (c.getY() + u * 2.9f).withHeight (u * 0.9f).toNearestInt(), Justification::centred, 1);

    if (shown == State::waitingBrowser && lic.getUserCode().isNotEmpty())
    {
        g.setColour (p.accent);
        g.setFont (NobLookAndFeel::font (u * 0.95f, true, 0.30f));
        g.drawText (lic.getUserCode(), text.withY (c.getY() + u * 3.95f).withHeight (u * 1.2f), Justification::centred);
        g.setColour (p.inkDim);
        g.setFont (NobLookAndFeel::font (u * 0.42f, false, 0.02f));
        g.drawFittedText (body, text.withY (c.getY() + u * 5.35f).withHeight (u * 1.6f).toNearestInt(), Justification::centredTop, 2);
    }
    else
    {
        g.setColour (shown == State::error ? p.accent : p.inkDim);
        g.setFont (NobLookAndFeel::font (u * 0.44f, false, 0.02f));
        g.drawFittedText (body, text.withY (c.getY() + u * 4.2f).withHeight (u * 2.6f).toNearestInt(), Justification::centredTop, 4);
    }
}

void LoginOverlay::Btn::paintButton (Graphics& g, bool isOver, bool isDown)
{
    const auto& p = palFor (*this);
    const bool dark = darkFor (*this);
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = b.getHeight() * 0.16f;

    Colour fill = isPrimary ? p.accent : (dark ? Colour (0xff262626) : Colour (0xffffffff));
    if (isOver) fill = fill.brighter (0.1f);
    if (isDown) fill = fill.darker (0.15f);
    g.setColour (fill);
    g.fillRoundedRectangle (b, corner);
    g.setColour (p.ink.withAlpha (isPrimary ? 0.0f : 0.25f));
    g.drawRoundedRectangle (b, corner, 1.0f);

    g.setColour (isPrimary ? (dark ? Colours::white : Colour (0xff1a0903)) : p.ink);
    g.setFont (NobLookAndFeel::font (b.getHeight() * 0.34f, true, 0.18f));
    g.drawFittedText (getButtonText(), b.toNearestInt().reduced (4, 0), Justification::centred, 1);
}
