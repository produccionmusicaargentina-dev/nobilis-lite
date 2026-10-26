#pragma once

#include "LiteProcessor.h"
#include "AdepsEngine.h"

// Stable parameter IDs (never rename; add new ones at the end)
namespace adeps::ID
{
    inline constexpr const char* fatness  = "fatness";
    inline constexpr const char* color    = "color";
    inline constexpr const char* gain     = "gain";
    inline constexpr const char* autoGain = "auto_gain";
}

class AdepsProcessor : public nob::LiteProcessor
{
public:
    AdepsProcessor();
    juce::AudioProcessorEditor* createEditor() override;

    adeps::Engine engine;
    adeps::Params currentParams() const noexcept;

protected:
    void prepareEffect (double sampleRate, int maxBlock) override;
    void processEffect (float* L, float* R, int numSamples) override;
    int getEffectLatency() const override { return engine.getLatency(); }
    float getEffectMakeupDb() const override { return engine.getMakeupDb(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    Raw rFat, rColor, rGain, rAuto;
};
