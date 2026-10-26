#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LicenseManager.h"

/** Cartel que tapa el plugin mientras no hay sesión iniciada. */
class LoginOverlay : public juce::Component,
                     private juce::ChangeListener
{
public:
    explicit LoginOverlay (nobilis::LicenseManager&);
    ~LoginOverlay() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Btn : public juce::Button
    {
    public:
        Btn (const juce::String& n, bool primary) : juce::Button (n), isPrimary (primary) {}
        void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
        bool isPrimary;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
    juce::Rectangle<float> card() const;

    nobilis::LicenseManager& lic;
    Btn primary { "SIGN IN FREE", true }, secondary { "CREATE ACCOUNT", false };
    nobilis::LicenseManager::State shown = nobilis::LicenseManager::State::checking;
};
