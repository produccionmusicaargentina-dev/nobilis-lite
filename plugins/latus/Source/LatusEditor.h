#pragma once

#include "ui/LiteEditor.h"
#include "LatusProcessor.h"

// Stereo field (vectorscope, mid up / side across): you see the chorus open the image.
class FieldScreen : public nob::ui::GlassScreen
{
public:
    explicit FieldScreen (LatusProcessor& p) : proc (p) {}
    void update();
protected:
    void paintScreen (juce::Graphics&, juce::Rectangle<float>, const NobPalette&) override;
private:
    LatusProcessor& proc;
    std::array<float, latus::Engine::scopeSize> l {}, r {};
    float scale = 1.0f, correlation = 1.0f;
};

class LatusEditor : public nob::ui::LiteEditor
{
public:
    explicit LatusEditor (LatusProcessor&);

protected:
    void layoutBody() override;
    void paintBody (juce::Graphics&, const NobPalette&, bool dark) override;
    void bodyTimer() override;
    void refreshTexts() override;

private:
    LatusProcessor& proc;
    FieldScreen screen { proc };
    nob::ui::HeroKnob width;
    nob::ui::ChoiceKeys mode;
    nob::ui::Knob speed, age, mix, output;
    nob::ui::Toggle bassMono;
};
