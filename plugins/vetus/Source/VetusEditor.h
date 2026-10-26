#pragma once

#include "ui/LiteEditor.h"
#include "VetusProcessor.h"

// The medium, drawn in light: tape reels, a record, a sampler LCD or a radio dial.
class MediumScreen : public nob::ui::GlassScreen
{
public:
    explicit MediumScreen (VetusProcessor& p) : proc (p) {}
    void update();
protected:
    void paintScreen (juce::Graphics&, juce::Rectangle<float>, const NobPalette&) override;
private:
    void drawTape (juce::Graphics&, juce::Rectangle<float>, const NobPalette&, float rot, float age);
    void drawVinyl (juce::Graphics&, juce::Rectangle<float>, const NobPalette&, float rot, float age);
    void drawSampler (juce::Graphics&, juce::Rectangle<float>, const NobPalette&, float rot, float age);
    void drawRadio (juce::Graphics&, juce::Rectangle<float>, const NobPalette&, float rot, float age);
    VetusProcessor& proc;
    float rotation = 0.0f;
};

class VetusEditor : public nob::ui::LiteEditor
{
public:
    explicit VetusEditor (VetusProcessor&);

protected:
    void layoutBody() override;
    void paintBody (juce::Graphics&, const NobPalette&, bool dark) override;
    void bodyTimer() override;
    void refreshTexts() override;

private:
    VetusProcessor& proc;
    MediumScreen screen { proc };
    nob::ui::HeroKnob age;
    nob::ui::ChoiceKeys medium;
    nob::ui::Knob noise, wobble, mix, output;
    nob::ui::Toggle gate;
};
