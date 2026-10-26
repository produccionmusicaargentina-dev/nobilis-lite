#pragma once

#include "ui/LiteEditor.h"
#include "PulsusProcessor.h"

// The groove display: one bar (or the whole step, if longer) of the gain curve,
// with the playhead running across it.
class GrooveScreen : public nob::ui::GlassScreen
{
public:
    explicit GrooveScreen (PulsusProcessor& p) : proc (p) {}
    void update();
protected:
    void paintScreen (juce::Graphics&, juce::Rectangle<float>, const NobPalette&) override;
private:
    PulsusProcessor& proc;
    float playhead = 0.0f;
};

class PulsusEditor : public nob::ui::LiteEditor
{
public:
    explicit PulsusEditor (PulsusProcessor&);

protected:
    void layoutBody() override;
    void paintBody (juce::Graphics&, const NobPalette&, bool dark) override;
    void bodyTimer() override;
    void refreshTexts() override;

private:
    PulsusProcessor& proc;
    GrooveScreen screen { proc };
    nob::ui::HeroKnob depth;
    nob::ui::ChoiceKeys rate, shape, band;
    nob::ui::Knob shift, breathe, xover, mix, output;
    juce::Rectangle<float> readout;
    juce::String shownTempo;
};
