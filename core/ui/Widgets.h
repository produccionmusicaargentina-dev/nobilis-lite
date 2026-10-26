#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "NobLookAndFeel.h"

//==============================================================================
// Building blocks shared by the four Lite interfaces. Everything is drawn in
// the NOX / LUX palette of NobLookAndFeel, so the theme switch just repaints.
//==============================================================================
namespace nob::ui
{

const NobPalette& palOf (juce::Component&);
bool darkOf (juce::Component&);

//==============================================================================
// Rotary knob + engraved caption above + range captions below (IGNIS / ACIES style).
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption,
          const juce::String& minText = {}, const juce::String& maxText = {}, bool bipolar = false);

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::String caption, minText, maxText;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    juce::Rectangle<float> knobArea;
};

//==============================================================================
// The big knob in the middle of each plugin: fluted cap + a ring of LED
// segments that light up with the value (like the DRIVE of IGNIS) + 0..10.
class HeroKnob : public juce::Component,
                 private juce::Timer
{
public:
    HeroKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption);

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::String caption;
    // optional: brightness 0..1 of an extra "activity" glow (e.g. the pump), polled by the timer
    std::function<float()> activity;
    // optional: names printed around the scale instead of 0..10 (e.g. COLOR: LADDER / OTA / SALLEN-KEY)
    juce::StringArray scaleNames;

private:
    void timerCallback() override;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    juce::Rectangle<float> knobArea;
    float shownActivity = 0.0f;
};

//==============================================================================
// Backlit keys bound to a choice parameter (one lit at a time).
class ChoiceKeys : public juce::Component
{
public:
    ChoiceKeys (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
                const juce::StringArray& labels, int columns = 0);
    ~ChoiceKeys() override;

    void resized() override;
    void setGap (float g) { gap = g; resized(); }

private:
    void sync();
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::OwnedArray<StyleButton> keys;
    int columns = 0, current = -1;
    float gap = 6.0f;
};

//==============================================================================
// One backlit key bound to a bool parameter.
class ToggleKey : public StyleButton
{
public:
    ToggleKey (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

//==============================================================================
// Chrome bat toggle with an engraved caption under it, bound to a bool parameter.
class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption);
    void paint (juce::Graphics&) override;
    void resized() override;
    BatToggle bat;
    juce::String caption;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

//==============================================================================
// Dark glass display, lit in the theme colour. Subclasses draw the content.
class GlassScreen : public juce::Component
{
public:
    void paint (juce::Graphics&) final;
protected:
    virtual void paintScreen (juce::Graphics&, juce::Rectangle<float> area, const NobPalette&) = 0;
    juce::String label, rightLabel;   // small captions in the top corners
};

//==============================================================================
// Engraved section caption with the accent dash, as on ACIES ("— DYNAMICS").
void drawSectionTitle (juce::Graphics&, const NobPalette&, juce::Rectangle<float> area, const juce::String& text,
                       juce::Justification = juce::Justification::centredLeft);

// Recessed plate behind a group of controls.
void drawPlate (juce::Graphics&, const NobPalette&, bool dark, juce::Rectangle<float> area);

// Small round lamp (on / off) with glow.
void drawLamp (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour colour, float level);

} // namespace nob::ui
