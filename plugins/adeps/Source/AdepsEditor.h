#pragma once

#include "ui/LiteEditor.h"
#include "AdepsProcessor.h"

// Transfer curve of the shaper (what FATNESS and COLOR do to the waveform),
// the live input level on it, and the glue compressor's pressure underneath.
class TransferScreen : public nob::ui::GlassScreen
{
public:
    explicit TransferScreen (AdepsProcessor& p) : proc (p) {}
    void update();
protected:
    void paintScreen (juce::Graphics&, juce::Rectangle<float>, const NobPalette&) override;
private:
    AdepsProcessor& proc;
    float inLevel = 0.0f, gr = 0.0f;
    juce::String key;
};

class AdepsEditor : public nob::ui::LiteEditor
{
public:
    explicit AdepsEditor (AdepsProcessor&);

protected:
    void layoutBody() override;
    void paintBody (juce::Graphics&, const NobPalette&, bool dark) override;
    void paintLogo (juce::Graphics&, juce::Rectangle<float>, const NobPalette&) override;
    void bodyTimer() override;
    void refreshTexts() override;

private:
    AdepsProcessor& proc;
    TransferScreen screen { proc };
    nob::ui::HeroKnob fatness, color;
    nob::ui::Knob gain, mix, output;
    nob::ui::Toggle autoGain;
    struct Lamp : public juce::Component
    {
        juce::Colour colour; float level = 0.0f;
        void set (juce::Colour c, float l) { if (c != colour || std::abs (l - level) > 0.02f) { colour = c; level = l; repaint(); } }
        void paint (juce::Graphics& g) override { nob::ui::drawLamp (g, getLocalBounds().toFloat().getCentre(), 7.0f, colour, level); }
    } lamp;
    float shownFat = -1.0f;
};
